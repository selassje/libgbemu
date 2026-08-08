module gbemu;

namespace {

constexpr std::uint16_t EXT_RAM_START = 0xA000;

}

namespace gbemu {

Mapper::Mapper(std::span<const std::uint8_t> rom)
  : m_rom(rom.begin(), rom.end())
{
}

std::uint8_t
Mapper::readRam(std::uint16_t address) const
{
  return m_ram.at(address - EXT_RAM_START);
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  m_ram.at(address - EXT_RAM_START) = value;
}

}
