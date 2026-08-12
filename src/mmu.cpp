module gbemu;

namespace gbemu {

#ifdef ENABLE_TESTS
std::string&
serialOutput()
{
  static std::string value;
  return value;
}

std::string&
memoryOutput()
{
  static std::string value;
  return value;
}
#endif

template<typename Self>
auto&
Mmu::getByteRef(this Self& self, std::uint16_t address)
{
  // ROM (0x0000-0x7FFF) and external RAM (0xA000-0xBFFF) are handled
  // directly by readByte()/writeByte() instead, via the mapper - see
  // their own comments. Both callers already guarantee address is
  // outside those two ranges before reaching here.
  if (address < 0xA000) {
    return self.m_vram.at(address - (2 * KB16) +
                          (self.m_switchableVRamBank * KB8));
  }

  if (address < 0xD000) {
    return self.m_wram.at(address - 0xC000);
  }

  if (address < 0xE000) {
    return self.m_wram.at(address - 0xD000 + (self.m_switchableWRamBank * KB4));
  }

  if (address < 0xFE00) {
    return self.getByteRef(address - (0xE000 - 0xC000));
  }

  if (address < 0xFEA0) {
    return self.m_oam.at(address - 0xFE00);
  }

  if (address < 0xFF00) {
    return self.m_unusable;
  }

  if (address < 0xFF80) {
    return self.m_io.at(address - 0xFF00);
  }

  if (address < 0xFFFF) {
    return self.m_hram.at(address - 0xFF80);
  }
  return self.m_interruptEnableRegister;
}

namespace {

constexpr std::uint16_t BOOT_ROM_FIRST_PART_END = 0x100;
constexpr std::uint16_t BOOT_ROM_SECOND_PART_START = 0x200;
constexpr std::uint16_t BOOT_ROM_SECOND_PART_END = 0x900;
constexpr std::uint8_t INTERRUPT_FLAGS_UNUSED_BITS = 0xE0;

constexpr unsigned STAT_INTERRUPT_FLAG_BIT = 0b0000'0010U;

constexpr std::uint16_t APU_REGISTERS_START = 0xFF10;

constexpr std::uint16_t WAVE_RAM_SIZE = 16;

// A release-mode-active assertion for internal invariants that genuinely
// should be unreachable (as opposed to std::expected, this codebase's usual
// error-reporting path for conditions a caller can legitimately hit, like a
// malformed ROM or save state) - plain <cassert>/assert() compiles out under
// NDEBUG (every Release preset here defines it), which would silently turn
// a real logic bug into undefined behavior instead of a diagnosable crash.
// Deliberately not a thrown exception either: nothing up the call stack
// from readByte()/writeByte() (called on every single memory access) has a
// try/catch the way GameBoy::loadState() does for SaveStateReader's own
// throws, so an uncaught exception here would just crash with no
// diagnostic. std::abort() is what a debugger/crash dump can actually
// point at the real failure site.
void
hardAssert(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "gbemu: internal error: " << message << '\n';
    std::abort();
  }
}

}

std::uint8_t
Mmu::readByte(std::uint16_t address) const
{
  if (m_bootRomActive) {
    const bool inFirstPart = address < BOOT_ROM_FIRST_PART_END;
    const bool inSecondPart = m_bootRom.size() > BOOT_ROM_FIRST_PART_END &&
                              address >= BOOT_ROM_SECOND_PART_START &&
                              address < BOOT_ROM_SECOND_PART_END;
    if (inFirstPart || inSecondPart) {
      return m_bootRom.at(address);
    }
  }
  // ROM - delegated to the mapper (see mapper.cppm's own comments on why
  // this needs the whole 0x0000-0x7FFF range, not just 0x4000-0x7FFF).
  if (address < 0x8000) {
    return std::visit(
      [address](const auto& mapper) { return mapper.readRom(address); },
      m_mapper);
  }
  // External RAM - also delegated to the mapper.
  if (address >= 0xA000 && address < 0xC000) {
    return std::visit(
      [address](const auto& mapper) { return mapper.readRam(address); },
      m_mapper);
  }
  // getByteRef()'s explicit-object-parameter overload deduces Self as
  // const Mmu here (readByte() is const), so this returns a
  // const std::uint8_t& - no const_cast needed to reuse it read-only.
  const auto value = getByteRef(address);
  if (address == regs::IF) {
    return static_cast<std::uint8_t>(
      static_cast<unsigned>(value) |
      static_cast<unsigned>(INTERRUPT_FLAGS_UNUSED_BITS));
  }
  if (address == regs::KEY1) {
    if (!m_isCgbHardware) {
      return 0xFF;
    }
    constexpr unsigned currentSpeedBit = 0x80U;
    constexpr unsigned unusedBits = 0x7EU;
    constexpr unsigned prepareBit = 0x01U;
    return static_cast<std::uint8_t>((m_doubleSpeed ? currentSpeedBit : 0U) |
                                     unusedBits |
                                     (m_speedSwitchPrepared ? prepareBit : 0U));
  }
  // DIV is just the upper 8 bits of the real 16-bit divider counter - see
  // divCounter(). Not stored in m_io at all (unlike most registers), so
  // this ignores the generic getByteRef()-derived value entirely.
  if (address == regs::DIV) {
    constexpr unsigned divCounterHighByteShift = 8U;
    return static_cast<std::uint8_t>(static_cast<unsigned>(m_divCounter) >>
                                     divCounterHighByteShift);
  }
  // Bits 6-7 are unused (always 1). Bits 0-3 are active-low button inputs:
  // start with all released (1) and clear a bit for each pressed button in
  // whichever group(s) bits 4-5 currently select - real hardware wire-ANDs
  // both groups together if both are selected simultaneously, which this
  // sequential clearing naturally reproduces.
  if (address == regs::JOYP) {
    constexpr unsigned dpadSelectBit = 0b0001'0000U;
    constexpr unsigned buttonSelectBit = 0b0010'0000U;
    constexpr unsigned selectionBits = 0b0011'0000U;
    constexpr unsigned unusedBits = 0b1100'0000U;
    constexpr unsigned lowNibbleMask = 0b0000'1111U;

    unsigned releasedBits = lowNibbleMask;
    const auto pressed = static_cast<unsigned>(m_buttonState);
    if ((static_cast<unsigned>(value) & dpadSelectBit) == 0) {
      releasedBits &= ~(pressed & lowNibbleMask);
    }
    if ((static_cast<unsigned>(value) & buttonSelectBit) == 0) {
      releasedBits &= ~((pressed >> 4U) & lowNibbleMask);
    }
    return static_cast<std::uint8_t>(
      unusedBits | (static_cast<unsigned>(value) & selectionBits) |
      releasedBits);
  }
  // NR14/NR24/NR34/NR44 ("period high & control", or plain "control" for
  // ch4, which has no period bits of its own): trigger (bit 7) and, where
  // present, period-high (bits 2-0) are write-only, bits 5-3 are unused -
  // only length enable (bit 6) is ever readable back.
  if (address == regs::NR14 || address == regs::NR24 || address == regs::NR34 ||
      address == regs::NR44) {
    constexpr unsigned lengthEnableBit = 0b0100'0000U;
    constexpr unsigned everythingElseReadsAsOne = 0b1011'1111U;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & lengthEnableBit) |
      everythingElseReadsAsOne);
  }
  // NR11/NR21 ("length timer & duty cycle"): duty (bits 7-6) is readable,
  // the initial length timer (bits 5-0) is write-only.
  if (address == regs::NR11 || address == regs::NR21) {
    constexpr unsigned dutyBits = 0b1100'0000U;
    constexpr unsigned lengthBitsReadAsOne = 0b0011'1111U;
    return static_cast<std::uint8_t>((static_cast<unsigned>(value) & dutyBits) |
                                     lengthBitsReadAsOne);
  }
  // NR13/NR23/NR33 (period low) and NR31/NR41 (length timer) are entirely
  // write-only.
  if (address == regs::NR13 || address == regs::NR23 || address == regs::NR33 ||
      address == regs::NR31 || address == regs::NR41) {
    constexpr std::uint8_t writeOnlyReadsAsAllOnes = 0xFF;
    return writeOnlyReadsAsAllOnes;
  }
  // 0xFF15 ("NR20") and 0xFF1F ("NR40") aren't real registers at all - the
  // gaps between NR14/NR21 and NR34/NR41 respectively. Unlike the
  // write-only registers above (which DO store a real value, just don't
  // expose it), real hardware has nothing wired up here, so these always
  // read back $FF regardless of what's written.
  constexpr std::uint16_t unusedNr20 = 0xFF15;
  constexpr std::uint16_t unusedNr40 = 0xFF1F;
  if (address == unusedNr20 || address == unusedNr40) {
    constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
    return unwiredReadsAsAllOnes;
  }
  // NR10: only sweep pace/direction/step (bits 6-0) are meaningful; bit 7
  // is unused.
  if (address == regs::NR10) {
    constexpr unsigned unusedBit = 0b1000'0000U;
    return static_cast<std::uint8_t>(static_cast<unsigned>(value) | unusedBit);
  }
  // NR30: only DAC on/off (bit 7) is meaningful; bits 6-0 are unused.
  if (address == regs::NR30) {
    constexpr unsigned dacEnabledBit = 0b1000'0000U;
    constexpr unsigned unusedBits = 0b0111'1111U;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & dacEnabledBit) | unusedBits);
  }
  // NR32: only output level (bits 6-5) is meaningful; bit 7 and bits 4-0
  // are unused.
  if (address == regs::NR32) {
    constexpr unsigned outputLevelBits = 0b0110'0000U;
    constexpr unsigned unusedBits = 0b1001'1111U;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & outputLevelBits) | unusedBits);
  }
  // NR52's CPU-visible value depends on live per-channel state (bits 0-3),
  // not just a fixed positional bitmask - Apu computes the whole byte.
  if (address == regs::NR52) {
    return m_apu.get().readRegister(address);
  }
  // CGB-only: bit 0 selects the VRAM bank, bits 1-7 always read back as 1.
  // Real DMG hardware doesn't have this register at all, always reading
  // $FF regardless of what's written - see setCgbMode().
  if (address == regs::VBK) {
    if (!m_isCgbHardware) {
      constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
      return unwiredReadsAsAllOnes;
    }
    constexpr unsigned vramBankBit = 0b0000'0001U;
    constexpr unsigned unusedBits = 0b1111'1110U;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & vramBankBit) | unusedBits);
  }
  // CGB-only: bits 0-2 select the WRAM bank, bits 3-7 always read back as
  // 1. Real DMG hardware doesn't have this register at all - see
  // setCgbMode().
  if (address == regs::SVBK) {
    if (!m_isCgbHardware) {
      constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
      return unwiredReadsAsAllOnes;
    }
    constexpr unsigned wramBankBits = 0b0000'0111U;
    constexpr unsigned unusedBits = 0b1111'1000U;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & wramBankBits) | unusedBits);
  }
  // CGB-only: bits 0-5 are the palette RAM index, bit 6 always reads back
  // as 1, bit 7 is the auto-increment flag. Real DMG hardware doesn't have
  // BCPS/OCPS at all - see setCgbMode().
  if (address == regs::BCPS || address == regs::OCPS) {
    if (!m_isCgbHardware) {
      constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
      return unwiredReadsAsAllOnes;
    }
    constexpr unsigned unusedBit = 0b0100'0000U;
    return static_cast<std::uint8_t>(static_cast<unsigned>(value) | unusedBit);
  }
  if (address == regs::OPRI) {
    if (!m_isCgbHardware) {
      return 0xFF;
    }
    constexpr unsigned priorityModeBit = 0x01U;
    constexpr unsigned unusedBits = 0xFEU;
    return static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) & priorityModeBit) | unusedBits);
  }
  // CGB-only: reads back the byte of BG/object palette RAM BCPS/OCPS
  // currently indexes - see bgPaletteColor()/objPaletteColor(). Real DMG
  // hardware doesn't have BCPD/OCPD at all.
  if (address == regs::BCPD || address == regs::OCPD) {
    if (!m_isCgbHardware) {
      constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
      return unwiredReadsAsAllOnes;
    }
    constexpr unsigned indexMask = 0b0011'1111U;
    const auto controlAddress = address == regs::BCPD ? regs::BCPS : regs::OCPS;
    const auto index =
      static_cast<unsigned>(getByteRef(controlAddress)) & indexMask;
    const auto& paletteRam =
      address == regs::BCPD ? m_bgPaletteRam : m_objPaletteRam;
    return paletteRam.at(index);
  }
  // 0xFF27-0xFF2F: the unused gap between NR52 and Wave RAM - like
  // 0xFF15/0xFF1F above, nothing real hardware wires up here, so it
  // always reads back $FF regardless of what's written.
  constexpr std::uint16_t unusedGapStart = 0xFF27;
  if (address >= unusedGapStart && address < regs::WAVE_RAM_START) {
    constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
    return unwiredReadsAsAllOnes;
  }
  // Wave RAM's CPU-visible value depends on live CH3 playback state (see
  // Apu::readWaveRam()) while the channel is enabled on DMG, not just the
  // stored byte.
  if (address >= regs::WAVE_RAM_START &&
      address < regs::WAVE_RAM_START + WAVE_RAM_SIZE) {
    return m_apu.get().readWaveRam(address);
  }
  // FF51-FF54 are write-only on real hardware (matching NR13/NR23/NR33/
  // NR31/NR41 above - they never expose their stored value back to the
  // CPU), true on both DMG (which doesn't have these registers at all) and
  // CGB hardware alike - no m_isCgbHardware check needed, unlike VBK/SVBK/
  // BCPS/OPRI/BCPD/OCPD below. FF55 falls through to the same unconditional
  // $FF when no HDMA transfer is active (never started, DMG, or GDMA -
  // synchronous, so software can never observe it in progress here either
  // way); the in-progress case (real HDMA, bit 7 clear + remaining length
  // in blocks in bits 0-6) is handled separately below.
  if (address >= regs::CGB_DMA_1 && address <= regs::CGB_DMA_5) {
    if (address == regs::CGB_DMA_5 && m_cgbDmaState.isHDMA &&
        m_cgbDmaState.bytesRemaining > 0) {
      // The CPU is stalled for the whole duration of a block transfer (see
      // Cpu::isCgbHdmaActive()), so no instruction - including this read -
      // can execute while isHDMABlockInTransfer is true; and bytesRemaining
      // is only ever touched alongside an active block copy (see
      // runNextTCycle()), so between blocks it's always a whole multiple of
      // 0x10. Both conditions being false here would mean one of those
      // invariants broke elsewhere, not a real hardware state to encode.
      hardAssert(!m_cgbDmaState.isHDMABlockInTransfer,
                 "FF55 read while a HDMA block transfer is in progress");
      hardAssert(m_cgbDmaState.bytesRemaining >= 0x10,
                 "HDMA bytesRemaining isn't a whole block between transfers");
      return static_cast<std::uint8_t>((m_cgbDmaState.bytesRemaining / 0x10U) -
                                       1U);
    }
    constexpr std::uint8_t writeOnlyOrUnimplementedReadsAsAllOnes = 0xFF;
    return writeOnlyOrUnimplementedReadsAsAllOnes;
  }
  return value;
}

namespace {

std::uint16_t
paletteColor(const std::array<std::uint8_t, 64>& paletteRam,
             std::uint8_t palette,
             std::uint8_t colorIndex)
{
  constexpr std::size_t colorsPerPalette = 4;
  constexpr std::size_t bytesPerColor = 2;
  const auto offset =
    ((static_cast<std::size_t>(palette) * colorsPerPalette) + colorIndex) *
    bytesPerColor;
  const auto lowByte = paletteRam.at(offset);
  const auto highByte = paletteRam.at(offset + 1);
  return static_cast<std::uint16_t>((static_cast<unsigned>(highByte) << 8U) |
                                    static_cast<unsigned>(lowByte));
}

}

std::uint16_t
Mmu::bgPaletteColor(std::uint8_t palette, std::uint8_t colorIndex) const
{
  return paletteColor(m_bgPaletteRam, palette, colorIndex);
}

std::uint16_t
Mmu::objPaletteColor(std::uint8_t palette, std::uint8_t colorIndex) const
{
  return paletteColor(m_objPaletteRam, palette, colorIndex);
}

std::uint8_t
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mmu::readVram(std::uint8_t bank, std::uint16_t address) const
{
  constexpr unsigned bankMask = 0b1U;
  const auto bankIndex =
    static_cast<std::size_t>(static_cast<unsigned>(bank) & bankMask);
  constexpr std::uint16_t vramStart = 2 * KB16;
  return m_vram.at((address - vramStart) + (bankIndex * KB8));
}

std::uint16_t
Mmu::readWord(std::uint16_t address) const
{
  const auto lowByte = readByte(address);
  const auto highByte = readByte(address + 1);
  return static_cast<std::uint16_t>((static_cast<unsigned>(highByte) << 8U) |
                                    static_cast<unsigned>(lowByte));
}

void
Mmu::writeByte(std::uint16_t address, std::uint8_t value)
{
#ifdef ENABLE_TESTS
  if (address == 0xFF02 && value == 0x81) {
    const auto serialChar = static_cast<char>(readByte(0xFF01));
    serialOutput() += serialChar;
  }
  constexpr std::uint16_t textOutBase = 0xA004;
  constexpr std::uint16_t textOutEnd = 0xC000;
  if (address >= textOutBase && address < textOutEnd) {
    const auto offset = static_cast<std::size_t>(address - textOutBase);
    if (memoryOutput().size() <= offset) {
      memoryOutput().resize(offset + 1, '\0');
    }
    memoryOutput().at(offset) = static_cast<char>(value);
  }
#endif

  // Writes in the cartridge ROM area control the mapper (bank switching,
  // etc.) - ROM itself is never writable. cpu_instrs.gb is an MBC1
  // cartridge and uses this register to dispatch each of its individual
  // test banks.
  if (address < 0x8000) {
    std::visit(
      [address, value](auto& mapper) { mapper.writeRom(address, value); },
      m_mapper);
    return;
  }

  // External RAM - also delegated to the mapper.
  if (address >= 0xA000 && address < 0xC000) {
    std::visit(
      [address, value](auto& mapper) { mapper.writeRam(address, value); },
      m_mapper);
    return;
  }

  if (address == regs::KEY1) {
    if (m_isCgbHardware) {
      m_speedSwitchPrepared = (static_cast<unsigned>(value) & 0x01U) != 0;
    }
    return;
  }

  // One-way latch: once disabled, the boot ROM can never be re-mapped, even
  // by writing 0 afterward - only a power cycle (a fresh Mmu) undoes this.
  if (address == regs::BOOT_ROM_DISABLE && value != 0) {
    m_bootRomActive = false;
  }

  // Writing any value to DIV resets the entire 16-bit divider counter to 0
  // - not just the CPU-visible upper byte. The written value itself is
  // irrelevant. This is also the real mechanism behind the well-known
  // "DIV write glitch": TIMA/the frame sequencer watch specific bits of
  // this same counter for a falling edge, so forcing the whole thing to 0
  // counts as one if the watched bit happened to be set beforehand.
  if (address == regs::DIV) {
    m_divCounter = 0;
    return;
  }

  // JOYP bits 0-3 are inputs (button state), not writable by the CPU -
  // only bits 4-5 (which of the two button groups is selected) actually
  // are.
  if (address == regs::JOYP) {
    constexpr unsigned joypWritableMask = 0b0011'0000U;
    auto& joyp = getByteRef(address);
    joyp = static_cast<std::uint8_t>(
      (static_cast<unsigned>(joyp) & ~joypWritableMask) |
      (static_cast<unsigned>(value) & joypWritableMask));
    return;
  }

  // STAT bits 0-2 (PPU mode, LYC==LY coincidence) are read-only and driven
  // by Ppu via updateStatMode()/updateStatCoincidence(), not by the CPU -
  // only bits 3-6 (the per-source interrupt enables) are actually
  // CPU-writable.
  if (address == regs::STAT) {
    constexpr unsigned statWritableMask = 0b0111'1000U;
    auto& stat = getByteRef(address);
    stat = static_cast<std::uint8_t>(
      (static_cast<unsigned>(stat) & ~statWritableMask) |
      (static_cast<unsigned>(value) & statWritableMask));
    return;
  }

  // NR52 bit 7 gates the APU's power - bits 0-3 (channel status) and 4-6
  // (unused) are read-only, so writes to them are silently discarded,
  // matching real hardware ("writing to those does not enable or disable
  // the channels"). Powering off clears NR10-NR51 (0xFF10-0xFF25) here and
  // makes them read-only until powered back on; Wave RAM is unaffected.
  // Apu tracks the power bit and its own channel state itself now (see
  // readByte()), so NR52 no longer needs a raw byte stored in m_io.
  if (address == regs::NR52) {
    constexpr unsigned powerBit = 0b1000'0000U;
    const bool poweringOn = (static_cast<unsigned>(value) & powerBit) != 0;
    m_apuRegistersReadOnly = !poweringOn;
    if (!poweringOn) {
      for (std::uint16_t reg = APU_REGISTERS_START; reg < regs::NR52; ++reg) {
        getByteRef(reg) = 0;
      }
    }
    m_apu.get().writeRegister(address, value);
    return;
  }

  // While the APU is powered off, NR10-NR51 are read-only (see the NR52
  // handling above) - Wave RAM (0xFF30-0xFF3F) is deliberately excluded,
  // it's always writable regardless of APU power state. The four length-
  // timer registers (NR11/NR21/NR31/NR41) are a second, narrower
  // exception, but only on DMG/MGB ("monochrome models" - Pan Docs'
  // Audio_Registers.md, NR52's own footnote): real hardware's length-
  // counter load circuit bypasses the power gate entirely there - see
  // dmg_sound/11-regs after power.gb's own "While powered off, writes to
  // NR41 are NOT ignored" check. CGB hardware doesn't have this bypass;
  // writes to these registers while powered off are ignored the same as
  // every other NR10-NR51 register there (confirmed by cgb_sound/11-regs
  // after power.gb's own "Powering off should clear NR41" check, which
  // fails if this bypass applies unconditionally). For NR11/NR21
  // specifically (DMG/MGB only), only their length bits (0-5) bypass it;
  // the duty bits (6-7) sharing that same register are ordinary
  // NR10-NR51 bits, still read-only while off (confirmed by
  // dmg_sound/01-registers.gb's own "when off, should ignore writes to
  // registers" check) - so merge the new length bits into the existing
  // stored duty bits rather than letting the whole byte through.
  const bool isLengthTimerRegister =
    !m_isCgbHardware && (address == regs::NR11 || address == regs::NR21 ||
                         address == regs::NR31 || address == regs::NR41);
  if (address >= APU_REGISTERS_START && address < regs::NR52 &&
      m_apuRegistersReadOnly) {
    if (!isLengthTimerRegister) {
      return;
    }
    if (address == regs::NR11 || address == regs::NR21) {
      static constexpr unsigned lengthBitsMask = 0b0011'1111U;
      static constexpr unsigned dutyBitsMask = 0b1100'0000U;
      value = static_cast<std::uint8_t>(
        (static_cast<unsigned>(getByteRef(address)) & dutyBitsMask) |
        (static_cast<unsigned>(value) & lengthBitsMask));
    }
  }

  // Forward channel-register writes (NR10-NR44), NR50 (master volume) and
  // NR51 (panning) to Apu - it needs both for its own mixing - only
  // reached once we know the write wasn't just dropped by the read-only
  // guard above.
  if (address >= APU_REGISTERS_START && address < regs::NR52) {
    m_apu.get().writeRegister(address, value);
  }

  // Wave RAM mirrors into Apu on every write, unconditionally - unlike
  // NR10-NR51 above it's never read-only regardless of APU power state
  // (see the read-only guard above), so there's no gating to check first.
  if (address >= regs::WAVE_RAM_START &&
      address < regs::WAVE_RAM_START + WAVE_RAM_SIZE) {
    m_apu.get().writeWaveRam(address, value);
  }

  // Starts a new transfer, restarting any one already in progress - matches
  // real hardware. The register itself still stores the written byte
  // normally (falls through to getByteRef below), so reading 0xFF46 back
  // returns the last source page written.
  if (address == regs::OAM_DMA) {
    m_dmaState = DmaState{ .sourceBase = static_cast<std::uint16_t>(
                             static_cast<unsigned>(value) << 8U) };
  }

  // CGB-only: selects which VRAM bank 0x8000-0x9FFF maps to - see
  // setCgbMode() and getByteRef(). Ignored on DMG (no such bank to
  // select); the register itself still stores the written byte either way
  // (falls through to getByteRef below), it just never affects addressing
  // or reads back as anything but $FF there.
  if (address == regs::VBK && m_isCgbHardware) {
    constexpr unsigned vramBankBit = 0b0000'0001U;
    m_switchableVRamBank = static_cast<unsigned>(value) & vramBankBit;
  }

  // CGB-only: selects which WRAM bank 0xD000-0xDFFF maps to - bank 0
  // behaves as bank 1 (there's no way to map WRAM bank 0 into the
  // switchable window, only the fixed 0xC000-0xCFFF one). Ignored on DMG,
  // same reasoning as VBK above.
  if (address == regs::SVBK && m_isCgbHardware) {
    constexpr unsigned wramBankBits = 0b0000'0111U;
    auto bank = static_cast<unsigned>(value) & wramBankBits;
    if (bank == 0) {
      bank = 1;
    }
    m_switchableWRamBank = bank;
  }

  // CGB-only: writes the byte of BG palette RAM BCPS currently indexes,
  // then auto-increments that index (wrapping) if BCPS bit 7 is set - see
  // bgPaletteColor(). No backing byte for BCPD itself (like NR52, its
  // whole CPU-visible behavior is this side effect), so this returns
  // early rather than falling through to the generic getByteRef() store
  // below. On DMG, falls through and is simply not readable back as
  // anything but $FF (see readByte()) - the register doesn't exist there.
  if (address == regs::BCPD && m_isCgbHardware) {
    constexpr unsigned indexMask = 0b0011'1111U;
    constexpr unsigned autoIncBit = 0b1000'0000U;
    const auto bcps = static_cast<unsigned>(getByteRef(regs::BCPS));
    const auto index = bcps & indexMask;
    m_bgPaletteRam.at(index) = value;
    if ((bcps & autoIncBit) != 0) {
      const auto nextIndex = (index + 1) % m_bgPaletteRam.size();
      getByteRef(regs::BCPS) =
        static_cast<std::uint8_t>((bcps & ~indexMask) | nextIndex);
    }
    return;
  }

  // CGB-only: same as BCPD above, but for object palette RAM via OCPS.
  if (address == regs::OCPD && m_isCgbHardware) {
    constexpr unsigned indexMask = 0b0011'1111U;
    constexpr unsigned autoIncBit = 0b1000'0000U;
    const auto ocps = static_cast<unsigned>(getByteRef(regs::OCPS));
    const auto index = ocps & indexMask;
    m_objPaletteRam.at(index) = value;
    if ((ocps & autoIncBit) != 0) {
      const auto nextIndex = (index + 1) % m_objPaletteRam.size();
      getByteRef(regs::OCPS) =
        static_cast<std::uint8_t>((ocps & ~indexMask) | nextIndex);
    }
    return;
  }

  // Only an internally-clocked transfer (bit 0 set) has a local timer to
  // complete it - see m_serialTCyclesRemaining's comment. The register
  // itself still stores the written byte normally (falls through to
  // getByteRef below) either way.
  constexpr unsigned transferStartBit = 0b1000'0000U;
  constexpr unsigned internalClockBit = 0b0000'0001U;
  if (address == regs::SC &&
      (static_cast<unsigned>(value) & transferStartBit) != 0 &&
      (static_cast<unsigned>(value) & internalClockBit) != 0) {
    constexpr std::uint16_t tCyclesPerByte = 4096;
    m_serialTCyclesRemaining = tCyclesPerByte;
  }

  if (m_isCgbHardware) {
    switch (address) {
      // HDMA1/HDMA3 are the *high* byte of source/dest respectively, and
      // HDMA2/HDMA4 the *low* byte (confirmed against gdma_addr_mask.asm's
      // own register writes: HIGH(SrcBuf) -> rHDMA1, LOW(SrcBuf) -> rHDMA2)
      // - only the low-byte registers get the 0x10-byte-alignment mask.
      case regs::CGB_DMA_1:
        m_cgbDmaState.sourceHigh = value;
        break;
      case regs::CGB_DMA_2:
        m_cgbDmaState.sourceLow =
          static_cast<std::uint8_t>(static_cast<unsigned>(value) & 0xF0U);
        break;
      case regs::CGB_DMA_3:
        m_cgbDmaState.destHigh =
          static_cast<std::uint8_t>(static_cast<unsigned>(value) & 0x1FU);
        break;
      case regs::CGB_DMA_4:
        m_cgbDmaState.destLow =
          static_cast<std::uint8_t>(static_cast<unsigned>(value) & 0xF0U);
        break;
      case regs::CGB_DMA_5: {
        constexpr unsigned modeBit = 0x80U;
        constexpr unsigned lengthMask = 0x7FU;
        constexpr unsigned blockSize = 0x10U;
        m_cgbDmaState.isHDMA = (static_cast<unsigned>(value) & modeBit) != 0;
        const auto lengthInBlocks = static_cast<unsigned>(value) & lengthMask;
        m_cgbDmaState.bytesRemaining =
          static_cast<std::uint16_t>((lengthInBlocks + 1) * blockSize);
        break;
      }
      default:
        break;
    }
  }
  getByteRef(address) = value;
}

bool
Mmu::switchSpeed()
{
  if (!m_isCgbHardware || !m_speedSwitchPrepared) {
    return false;
  }
  m_doubleSpeed = !m_doubleSpeed;
  m_speedSwitchPrepared = false;
  m_divCounter = 0;
  return true;
}

void
Mmu::runNextTCycle()
{
  std::visit([](auto& mapper) { mapper.runNextTCycle(); }, m_mapper);

  ++m_divCounter;

  if (m_dmaState.has_value()) {
    auto& dma = *m_dmaState;
    constexpr std::uint8_t tCyclesPerByte = 4;
    ++dma.tCyclesSinceLastByte;
    if (dma.tCyclesSinceLastByte >= tCyclesPerByte) {
      dma.tCyclesSinceLastByte = 0;

      constexpr std::uint16_t oamBase = 0xFE00;
      getByteRef(static_cast<std::uint16_t>(oamBase + dma.offset)) =
        readByte(static_cast<std::uint16_t>(dma.sourceBase + dma.offset));

      constexpr std::uint8_t totalBytes = 160;
      ++dma.offset;
      if (dma.offset >= totalBytes) {
        m_dmaState.reset();
      }
    }
  }

  // GDMA copies continuously while any bytes remain; HDMA only copies
  // while isHDMABlockInTransfer (one 16-byte block per H-Blank - see
  // notifyHBlankStart()). Gating the WHOLE per-byte step below on this -
  // not just the writeByte() call - is what keeps a pending HDMA's
  // addresses/bytesRemaining frozen while the CPU runs normally between
  // blocks: advancing them unconditionally here would silently drain the
  // whole transfer's byte count during ordinary CPU execution, long before
  // the H-Blanks it actually needs have happened.
  const bool dmaActive =
    m_cgbDmaState.bytesRemaining > 0 &&
    (!m_cgbDmaState.isHDMA || m_cgbDmaState.isHDMABlockInTransfer);
  if (dmaActive) {
    constexpr std::uint8_t tCyclesPerByte = 2;
    ++m_cgbDmaState.tCyclesSinceLastByte;
    if (m_cgbDmaState.tCyclesSinceLastByte >= tCyclesPerByte) {
      m_cgbDmaState.tCyclesSinceLastByte = 0;

      const auto sourceAddress = m_cgbDmaState.sourceAddress();
      const auto destAddress = m_cgbDmaState.destAddress();
      writeByte(destAddress, readByte(sourceAddress));

      // Checked pre-decrement: bytesRemaining == 1 (mod 16) here means the
      // byte just written was the last of the current block.
      if (m_cgbDmaState.isHDMA && m_cgbDmaState.bytesRemaining % 16 == 1) {
        m_cgbDmaState.isHDMABlockInTransfer = false;
      }

      // 16-bit source/dest addresses, but sourceLow/destLow are only the
      // low byte - carry into the high byte on wraparound, same as any
      // normal 16-bit increment. Without this, a transfer crossing a
      // 256-byte boundary (common - max length is 0x800 bytes) would wrap
      // back to the start of the same page instead of advancing into the
      // next one.
      if (++m_cgbDmaState.sourceLow == 0) {
        ++m_cgbDmaState.sourceHigh;
      }
      if (++m_cgbDmaState.destLow == 0) {
        ++m_cgbDmaState.destHigh;
      }
      --m_cgbDmaState.bytesRemaining;
    }
  }

  if (m_serialTCyclesRemaining.has_value()) {
    --(*m_serialTCyclesRemaining);
    if (*m_serialTCyclesRemaining == 0) {
      m_serialTCyclesRemaining.reset();

      constexpr std::uint8_t noPartnerConnectedByte = 0xFF;
      constexpr unsigned transferStartBit = 0b1000'0000U;
      constexpr unsigned serialInterruptBit = 0b0000'1000U;

      getByteRef(regs::SB) = noPartnerConnectedByte;
      auto& sc = getByteRef(regs::SC);
      sc = static_cast<std::uint8_t>(static_cast<unsigned>(sc) &
                                     ~transferStartBit);
      auto& interruptFlags = getByteRef(regs::IF);
      interruptFlags = static_cast<std::uint8_t>(
        static_cast<unsigned>(interruptFlags) | serialInterruptBit);
    }
  }
}

void
Mmu::updateStatMode(std::uint8_t mode)
{
  constexpr unsigned modeMask = 0b0000'0011U;
  constexpr std::uint8_t hblankMode = 0;
  constexpr std::uint8_t vblankMode = 1;
  constexpr std::uint8_t oamMode = 2;
  constexpr unsigned hblankInterruptEnableBit = 0b0000'1000U;
  constexpr unsigned vblankInterruptEnableBit = 0b0001'0000U;
  constexpr unsigned oamInterruptEnableBit = 0b0010'0000U;

  auto& stat = getByteRef(regs::STAT);
  stat = static_cast<std::uint8_t>((static_cast<unsigned>(stat) & ~modeMask) |
                                   (static_cast<unsigned>(mode) & modeMask));

  // Mode 3 (PixelTransfer) has no STAT interrupt source on real hardware.
  unsigned modeInterruptEnableBit = 0;
  if (mode == hblankMode) {
    modeInterruptEnableBit = hblankInterruptEnableBit;
  } else if (mode == vblankMode) {
    modeInterruptEnableBit = vblankInterruptEnableBit;
  } else if (mode == oamMode) {
    modeInterruptEnableBit = oamInterruptEnableBit;
  }

  if (modeInterruptEnableBit != 0 &&
      (static_cast<unsigned>(stat) & modeInterruptEnableBit) != 0) {
    auto& interruptFlags = getByteRef(regs::IF);
    interruptFlags = static_cast<std::uint8_t>(
      static_cast<unsigned>(interruptFlags) | STAT_INTERRUPT_FLAG_BIT);
  }
}

void
Mmu::updateStatCoincidence(bool coincidence)
{
  constexpr unsigned coincidenceBit = 0b0000'0100U;
  constexpr unsigned lycInterruptEnableBit = 0b0100'0000U;

  auto& stat = getByteRef(regs::STAT);
  if (!coincidence) {
    stat =
      static_cast<std::uint8_t>(static_cast<unsigned>(stat) & ~coincidenceBit);
    return;
  }

  stat =
    static_cast<std::uint8_t>(static_cast<unsigned>(stat) | coincidenceBit);
  if ((static_cast<unsigned>(stat) & lycInterruptEnableBit) != 0) {
    auto& interruptFlags = getByteRef(regs::IF);
    interruptFlags = static_cast<std::uint8_t>(
      static_cast<unsigned>(interruptFlags) | STAT_INTERRUPT_FLAG_BIT);
  }
}

void
Mmu::setButtonState(Button button, bool pressed)
{
  const auto bit = 1U << static_cast<unsigned>(button);
  m_buttonState = static_cast<std::uint8_t>(
    pressed ? (static_cast<unsigned>(m_buttonState) | bit)
            : (static_cast<unsigned>(m_buttonState) & ~bit));
}

void
Mmu::writeWord(std::uint16_t address, std::uint16_t value)
{
  writeByte(address, static_cast<std::uint8_t>(value));
  writeByte(static_cast<std::uint16_t>(address + 1),
            static_cast<std::uint8_t>(static_cast<unsigned>(value) >> 8U));
}

void
Mmu::enableBootRom(std::span<const std::uint8_t> bootRom)
{
  m_bootRom.assign(bootRom.begin(), bootRom.end());
  m_bootRomActive = true;
}

std::expected<void, std::string>
Mmu::loadRom(std::span<const std::uint8_t> rom)
{
  if (rom.size() < MIN_ROM_SIZE) {
    return std::unexpected(
      "ROM size is too small. Must be at least 0x150 bytes.");
  }
  constexpr std::size_t cartridgeTypeAddress = 0x147;
  // Bounds already verified by the size check above (cartridgeTypeAddress
  // is well within MIN_ROM_SIZE).
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  const auto cartridgeType = rom[cartridgeTypeAddress];
  switch (cartridgeType) {
    case 0x00:
      m_mapper.emplace<RomOnlyMapper>(rom);
      break;
    case 0x01:
    case 0x02:
    case 0x03:
      m_mapper.emplace<Mbc1Mapper>(rom);
      break;
    case 0x0F:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
      m_mapper.emplace<Mbc3Mapper>(rom);
      break;
    case 0x19:
    case 0x1A:
    case 0x1B:
      m_mapper.emplace<Mbc5Mapper>(rom, false);
      break;
    case 0x1C:
    case 0x1D:
    case 0x1E:
      m_mapper.emplace<Mbc5Mapper>(rom, true);
      break;
    default:
      return std::unexpected(
        std::format("Unsupported cartridge type: 0x{:02X}", cartridgeType));
  }
  return {};
}

void
Mmu::serialize(SaveStateWriter& writer) const
{
  writer.writeBool(m_isCgbHardware);
  writer.writeBool(m_doubleSpeed);
  writer.writeBool(m_speedSwitchPrepared);

  writer.writeU8(m_cgbDmaState.sourceLow);
  writer.writeU8(m_cgbDmaState.sourceHigh);
  writer.writeU8(m_cgbDmaState.destLow);
  writer.writeU8(m_cgbDmaState.destHigh);
  writer.writeU16(m_cgbDmaState.bytesRemaining);
  writer.writeBool(m_cgbDmaState.isHDMA);
  writer.writeBool(m_cgbDmaState.isHDMABlockInTransfer);

  writer.writeBool(m_dmaState.has_value());
  if (m_dmaState) {
    writer.writeU16(m_dmaState->sourceBase);
    writer.writeU8(m_dmaState->offset);
    writer.writeU8(m_dmaState->tCyclesSinceLastByte);
  }

  writer.writeBool(m_serialTCyclesRemaining.has_value());
  if (m_serialTCyclesRemaining) {
    writer.writeU16(*m_serialTCyclesRemaining);
  }

  writer.writeU16(m_divCounter);
  writer.writeBool(m_bootRomActive);
  writer.writeBytes(m_vram);
  std::visit([&writer](const auto& mapper) { mapper.serialize(writer); },
             m_mapper);
  writer.writeBytes(m_wram);
  writer.writeBytes(m_oam);
  writer.writeBytes(m_io);
  writer.writeBytes(m_hram);
  writer.writeSize(m_switchableVRamBank);
  writer.writeSize(m_switchableWRamBank);
  writer.writeBytes(m_bgPaletteRam);
  writer.writeBytes(m_objPaletteRam);
  writer.writeU8(m_interruptEnableRegister);
  writer.writeU8(m_unusable);
  writer.writeU8(m_buttonState);
  writer.writeBool(m_apuRegistersReadOnly);
}

void
Mmu::deserialize(SaveStateReader& reader)
{
  m_isCgbHardware = reader.readBool();
  m_doubleSpeed = reader.readBool();
  m_speedSwitchPrepared = reader.readBool();

  m_cgbDmaState.sourceLow = reader.readU8();
  m_cgbDmaState.sourceHigh = reader.readU8();
  m_cgbDmaState.destLow = reader.readU8();
  m_cgbDmaState.destHigh = reader.readU8();
  m_cgbDmaState.bytesRemaining = reader.readU16();
  m_cgbDmaState.isHDMA = reader.readBool();
  m_cgbDmaState.isHDMABlockInTransfer = reader.readBool();

  if (reader.readBool()) {
    DmaState dmaState;
    dmaState.sourceBase = reader.readU16();
    dmaState.offset = reader.readU8();
    dmaState.tCyclesSinceLastByte = reader.readU8();
    m_dmaState = dmaState;
  } else {
    m_dmaState.reset();
  }

  if (reader.readBool()) {
    m_serialTCyclesRemaining = reader.readU16();
  } else {
    m_serialTCyclesRemaining.reset();
  }

  m_divCounter = reader.readU16();
  m_bootRomActive = reader.readBool();
  reader.readBytes(m_vram);
  std::visit([&reader](auto& mapper) { mapper.deserialize(reader); }, m_mapper);
  reader.readBytes(m_wram);
  reader.readBytes(m_oam);
  reader.readBytes(m_io);
  reader.readBytes(m_hram);
  m_switchableVRamBank = reader.readSize();
  m_switchableWRamBank = reader.readSize();
  reader.readBytes(m_bgPaletteRam);
  reader.readBytes(m_objPaletteRam);
  m_interruptEnableRegister = reader.readU8();
  m_unusable = reader.readU8();
  m_buttonState = reader.readU8();
  m_apuRegistersReadOnly = reader.readBool();
}

};
