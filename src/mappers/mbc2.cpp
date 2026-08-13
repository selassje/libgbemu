module gbemu;

namespace {

std::uint16_t
effectiveRamAddress(std::uint16_t address)
{
  return static_cast<std::uint16_t>(0xA000U + (address % 0x0200U));
}
}

namespace gbemu {

Mbc2Mapper::Mbc2Mapper(std::span<const std::uint8_t> rom)
  : Mapper(rom)
{
}

std::uint8_t
Mbc2Mapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    return romByte(address);
  }
  const auto bank = m_romBank % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc2Mapper::writeRom(std::uint16_t address, std::uint8_t value)
{
  if (address < 0x4000) {
    if ((address & 0x0100U) != 0) {
      m_romBank = value & 0x0FU;
      if (m_romBank == 0) {
        m_romBank = 1;
      }
      return;
    }
    m_ramEnabled = (value & 0x0FU) == 0x0AU;
    return;
  }
}

std::uint8_t
Mbc2Mapper::readRam(std::uint16_t address) const
{
  if (!m_ramEnabled) {
    return 0xFF;
  }

  const auto effectiveAddress = effectiveRamAddress(address);

  return Mapper::readRam(effectiveAddress);
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc2Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  if (!m_ramEnabled) {
    return;
  }
  const auto effectiveAddress = effectiveRamAddress(address);
  Mapper::writeRam(effectiveAddress, value);
}

void
Mbc2Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBank);
  writer.writeBool(m_ramEnabled);
  serializeRam(writer);
}

void
Mbc2Mapper::deserialize(SaveStateReader& reader)
{
  m_romBank = reader.readU8();
  m_ramEnabled = reader.readBool();
  deserializeRam(reader);
}

}
