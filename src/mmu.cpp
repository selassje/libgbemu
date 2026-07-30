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
