module gbemu;

namespace {

constexpr unsigned MBC1_BANK_HIGH_SHIFT = 5U;

}

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

std::uint8_t&
Mmu::getByteRef(std::uint16_t address)
{
  if (address < 0x4000) {
    std::size_t bank = 0;
    if (m_usesMbc1 && m_mbc1BankingMode) {
      bank = static_cast<std::size_t>(m_mbc1BankHigh) << MBC1_BANK_HIGH_SHIFT;
    }
    const auto bankCount = std::max<std::size_t>(1, m_rom.size() / KB16);
    bank %= bankCount;
    return m_rom.at((bank * KB16) + address);
  }
  if (address < 0x8000) {
    const auto bankCount = std::max<std::size_t>(1, m_rom.size() / KB16);
    const auto bank = m_switchableRomBank % bankCount;
    const std::size_t bankedAddress = (bank * KB16) + (address - KB16);
    return m_rom.at(bankedAddress);
  }
  if (address < 0xA000) {
    return m_vram.at(address - (2 * KB16) + (m_switchableVRamBank * KB8));
  }
  if (address < 0xC000) {
    return m_extRam.at(address - 0xA000);
  }

  if (address < 0xD000) {
    return m_wram.at(address - 0xC000);
  }

  if (address < 0xE000) {
    return m_wram.at(address - 0xD000 + (m_switchableWRamBank * KB4));
  }

  if (address < 0xFE00) {
    return getByteRef(address - (0xE000 - 0xC000));
  }

  if (address < 0xFEA0) {
    return m_oam.at(address - 0xFE00);
  }

  if (address < 0xFF00) {
    return m_unusable;
  }

  if (address < 0xFF80) {
    return m_io.at(address - 0xFF00);
  }

  if (address < 0xFFFF) {
    return m_hram.at(address - 0xFF80);
  }
  return m_interruptEnableRegister;
}

namespace {

constexpr std::uint16_t BOOT_ROM_FIRST_PART_END = 0x100;
constexpr std::uint16_t BOOT_ROM_SECOND_PART_START = 0x200;
constexpr std::uint16_t BOOT_ROM_SECOND_PART_END = 0x900;
constexpr std::uint8_t INTERRUPT_FLAGS_UNUSED_BITS = 0xE0;

constexpr std::uint16_t MBC1_ROM_BANK_REGISTER_START = 0x2000;
constexpr std::uint16_t MBC1_RAM_BANK_REGISTER_START = 0x4000;
constexpr std::uint16_t MBC1_BANKING_MODE_REGISTER_START = 0x6000;
constexpr unsigned MBC1_ROM_BANK_MASK = 0x1FU;
constexpr unsigned MBC1_BANK_HIGH_MASK = 0x03U;
constexpr unsigned MBC1_BANKING_MODE_MASK = 0x01U;

constexpr unsigned STAT_INTERRUPT_FLAG_BIT = 0b0000'0010U;

constexpr std::uint16_t APU_REGISTERS_START = 0xFF10;

constexpr std::uint16_t WAVE_RAM_SIZE = 16;

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
  // Safe: getByteRef() is only ever read through here, never written to, so
  // no actual mutation of a const object can occur regardless of whether
  // *this genuinely refers to a const Mmu.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  auto& self = const_cast<Mmu&>(*this);
  const auto value = self.getByteRef(address);
  if (address == regs::IF) {
    return static_cast<std::uint8_t>(
      static_cast<unsigned>(value) |
      static_cast<unsigned>(INTERRUPT_FLAGS_UNUSED_BITS));
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
  // 0xFF27-0xFF2F: the unused gap between NR52 and Wave RAM - like
  // 0xFF15/0xFF1F above, nothing real hardware wires up here, so it
  // always reads back $FF regardless of what's written.
  constexpr std::uint16_t unusedGapStart = 0xFF27;
  if (address >= unusedGapStart && address < regs::WAVE_RAM_START) {
    constexpr std::uint8_t unwiredReadsAsAllOnes = 0xFF;
    return unwiredReadsAsAllOnes;
  }
  return value;
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

  // Writes in the cartridge ROM area control the memory-bank controller; ROM
  // itself is never writable. cpu_instrs.gb is an MBC1 cartridge and uses
  // this register to dispatch each of its individual test banks.
  if (address < 0x8000) {
    if (m_usesMbc1) {
      const auto unsignedValue = static_cast<unsigned>(value);
      if (address >= MBC1_ROM_BANK_REGISTER_START &&
          address < MBC1_RAM_BANK_REGISTER_START) {
        m_mbc1RomBankLow =
          static_cast<std::uint8_t>(unsignedValue & MBC1_ROM_BANK_MASK);
        if (m_mbc1RomBankLow == 0) {
          m_mbc1RomBankLow = 1;
        }
      } else if (address >= MBC1_RAM_BANK_REGISTER_START &&
                 address < MBC1_BANKING_MODE_REGISTER_START) {
        m_mbc1BankHigh =
          static_cast<std::uint8_t>(unsignedValue & MBC1_BANK_HIGH_MASK);
      } else if (address >= MBC1_BANKING_MODE_REGISTER_START) {
        m_mbc1BankingMode = (unsignedValue & MBC1_BANKING_MODE_MASK) != 0;
      }
      m_switchableRomBank =
        (static_cast<std::size_t>(m_mbc1BankHigh) << MBC1_BANK_HIGH_SHIFT) |
        m_mbc1RomBankLow;
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
  // exception: real hardware's length-counter load circuit bypasses the
  // power gate entirely - see dmg_sound/11-regs after power.gb's own
  // "While powered off, writes to NR41 are NOT ignored" comment. But for
  // NR11/NR21 specifically, only their length bits (0-5) bypass it; the
  // duty bits (6-7) sharing that same register are ordinary NR10-NR51
  // bits, still read-only while off (confirmed by
  // dmg_sound/01-registers.gb's own "when off, should ignore writes to
  // registers" check) - so merge the new length bits into the existing
  // stored duty bits rather than letting the whole byte through.
  const bool isLengthTimerRegister =
    address == regs::NR11 || address == regs::NR21 || address == regs::NR31 ||
    address == regs::NR41;
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

  getByteRef(address) = value;
}

void
Mmu::runNextTCycle()
{
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
  m_rom.assign(rom.begin(), rom.end());
  constexpr std::size_t cartridgeTypeAddress = 0x147;
  const auto cartridgeType = m_rom.at(cartridgeTypeAddress);
  m_usesMbc1 =
    cartridgeType == 0x01 || cartridgeType == 0x02 || cartridgeType == 0x03;
  m_mbc1RomBankLow = 1;
  m_mbc1BankHigh = 0;
  m_mbc1BankingMode = false;
  m_switchableRomBank = 1;
  return {};
}

};
