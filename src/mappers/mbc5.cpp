
module gbemu;

namespace gbemu {

Mbc5Mapper::Mbc5Mapper(std::span<const std::uint8_t> rom, bool rumblerEnabled)
  : Mapper(rom)
  , m_rumblerEnabled(rumblerEnabled)
{
}

std::size_t
Mbc5Mapper::currentRomBank() const
{
  return (static_cast<std::size_t>(m_romBankHigh) << 8) |
         static_cast<std::size_t>(m_romBankLow);
}

std::uint8_t
Mbc5Mapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    return romByte(address);
  }
  const auto bank = currentRomBank() % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc5Mapper::writeRom(std::uint16_t address, std::uint8_t value)
{
  if (address < 0x2000) {
    m_ramEnabled = (value & 0x0FU) == 0x0AU;
    return;
  }
  if (address < 0x3000) {
    m_romBankLow = value;
    return;
  }

  if (address < 0x4000) {
    m_romBankHigh = value & 0x01U;
    return;
  }

  if (address < 0x6000) {
    m_ramBank = value & (m_rumblerEnabled ? 0x03U : 0x0FU);
    return;
  }
}

std::uint8_t
Mbc5Mapper::readRam(std::uint16_t address) const
{
  if (!m_ramEnabled) {
    return 0xFF;
  }
  return Mapper::readRam(address, m_ramBank % ramBankCount());
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc5Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  if (!m_ramEnabled) {
    return;
  }
  Mapper::writeRam(address, m_ramBank % ramBankCount(), value);
}

void
Mbc5Mapper::reset()
{
  resetRam();
  m_romBankLow = 1;
  m_romBankHigh = 0;
  m_ramEnabled = false;
  m_ramBank = 0;
}

void
Mbc5Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBankLow);
  writer.writeU8(m_romBankHigh);
  writer.writeBool(m_rumblerEnabled);
  writer.writeBool(m_ramEnabled);
  writer.writeU8(m_ramBank);
  serializeRam(writer);
}

void
Mbc5Mapper::deserialize(SaveStateReader& reader)
{
  m_romBankLow = reader.readU8();
  m_romBankHigh = reader.readU8();
  m_rumblerEnabled = reader.readBool();
  m_ramEnabled = reader.readBool();
  m_ramBank = reader.readU8();
  deserializeRam(reader);
}

}
