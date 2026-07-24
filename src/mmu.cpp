module gbemu;

namespace gbemu {

std::uint8_t&
Mmu::getByteRef(std::uint16_t address)
{
  if (address < 0x4000) {
    return m_rom.at(address);
  }
  if (address < 0x8000) {
    const std::size_t bankedAddress =
      (m_switchableRomBank * KB16) + (address - KB16);
    return m_rom.at(bankedAddress);
  }
  if (address < 0xA000) { // NOLINT(readability-magic-numbers)
    return m_vram.at(address - (2 * KB16) + (m_switchableVRamBank * KB8));
  }
  if (address < 0xC000) {                 // NOLINT(readability-magic-numbers)
    return m_extRam.at(address - 0xA000); // NOLINT(readability-magic-numbers)
  }

  if (address < 0xD000) {               // NOLINT(readability-magic-numbers)
    return m_wram.at(address - 0xC000); // NOLINT(readability-magic-numbers)
  }

  if (address < 0xE000) {             // NOLINT(readability-magic-numbers)
    return m_wram.at(address - 0xD000 // NOLINT(readability-magic-numbers)
                     + (m_switchableWRamBank * KB4));
  }

  if (address < 0xFE00) { // NOLINT(readability-magic-numbers)
    return getByteRef(address -
                      (0xE000 - 0xC000)); // NOLINT(readability-magic-numbers)
  }

  if (address < 0xFEA0) {              // NOLINT(readability-magic-numbers)
    return m_oam.at(address - 0xFE00); // NOLINT(readability-magic-numbers)
  }

  if (address < 0xFF00) { // NOLINT(readability-magic-numbers)
    return m_unusable;
  }

  if (address < 0xFF80) {             // NOLINT(readability-magic-numbers)
    return m_io.at(address - 0xFF00); // NOLINT(readability-magic-numbers)
  }

  if (address < 0xFFFF) {               // NOLINT(readability-magic-numbers)
    return m_hram.at(address - 0xFF80); // NOLINT(readability-magic-numbers)
  }
  return m_interruptEnableRegister;
}

std::uint8_t
Mmu::readByte(std::uint16_t address) const
{
  // Safe: getByteRef() is only ever read through here, never written to, so
  // no actual mutation of a const object can occur regardless of whether
  // *this genuinely refers to a const Mmu.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
  auto& self = const_cast<Mmu&>(*this);
  return self.getByteRef(address);
}

void
Mmu::writeByte(std::uint16_t address, std::uint8_t value)
{
  getByteRef(address) = value;
}

std::expected<void, std::string>
Mmu::loadRom(std::span<const std::uint8_t> rom)
{
  if (rom.size() < MIN_ROM_SIZE) {
    return std::unexpected("ROM size is too small. Must be at least 0x150 bytes.");	
  }
  m_rom.assign(rom.begin(), rom.end());
  return {};
}

};
