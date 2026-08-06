module gbemu;

namespace {

constexpr unsigned MBC1_BANK_HIGH_SHIFT = 5U;
constexpr std::uint16_t MBC1_ROM_BANK_REGISTER_START = 0x2000;
constexpr std::uint16_t MBC1_RAM_BANK_REGISTER_START = 0x4000;
constexpr std::uint16_t MBC1_BANKING_MODE_REGISTER_START = 0x6000;
constexpr unsigned MBC1_ROM_BANK_MASK = 0x1FU;
constexpr unsigned MBC1_BANK_HIGH_MASK = 0x03U;
constexpr unsigned MBC1_BANKING_MODE_MASK = 0x01U;
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

RomOnlyMapper::RomOnlyMapper(std::span<const std::uint8_t> rom)
  : Mapper(rom)
{
}

std::uint8_t
RomOnlyMapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    return romByte(address);
  }
  // No register ever changes which bank this reads (writeRom() is a
  // no-op) - always bank 1, the same fixed value Mmu's pre-refactor
  // m_switchableRomBank permanently held for a non-MBC1 cartridge.
  const auto bank = std::size_t{ 1 } % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
RomOnlyMapper::writeRom(std::uint16_t /*address*/, std::uint8_t /*value*/)
{
  // No registers - ROM ONLY cartridges have no bank switching.
}

void
RomOnlyMapper::reset()
{
  resetRam();
}

void
RomOnlyMapper::serialize(SaveStateWriter& writer) const
{
  serializeRam(writer);
}

void
RomOnlyMapper::deserialize(SaveStateReader& reader)
{
  deserializeRam(reader);
}

Mbc1Mapper::Mbc1Mapper(std::span<const std::uint8_t> rom)
  : Mapper(rom)
{
}

std::size_t
Mbc1Mapper::currentRomBank() const
{
  return (static_cast<std::size_t>(m_bankHigh) << MBC1_BANK_HIGH_SHIFT) |
         m_romBankLow;
}

std::uint8_t
Mbc1Mapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    // Advanced banking mode (m_bankingMode) remaps bank 0 too, using just
    // the high bits - a genuine real-hardware quirk (used by multicarts
    // and to reach banks 0x20/0x40/0x60, which m_romBankLow alone can
    // never select - see writeRom()), not an emulator invention.
    std::size_t bank = 0;
    if (m_bankingMode) {
      bank = static_cast<std::size_t>(m_bankHigh) << MBC1_BANK_HIGH_SHIFT;
    }
    bank %= bankCount;
    return romByte((bank * KB16) + address);
  }
  const auto bank = currentRomBank() % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc1Mapper::writeRom(std::uint16_t address, std::uint8_t value)
{
  const auto unsignedValue = static_cast<unsigned>(value);
  if (address < MBC1_ROM_BANK_REGISTER_START) {
    // RAM-enable register (0x0000-0x1FFF) - not modeled yet (see Mapper's
    // own comment on RAM always being accessible regardless of any
    // header/register gating).
    return;
  }
  if (address < MBC1_RAM_BANK_REGISTER_START) {
    m_romBankLow =
      static_cast<std::uint8_t>(unsignedValue & MBC1_ROM_BANK_MASK);
    if (m_romBankLow == 0) {
      m_romBankLow = 1;
    }
    return;
  }
  if (address < MBC1_BANKING_MODE_REGISTER_START) {
    m_bankHigh = static_cast<std::uint8_t>(unsignedValue & MBC1_BANK_HIGH_MASK);
    return;
  }
  m_bankingMode = (unsignedValue & MBC1_BANKING_MODE_MASK) != 0;
}

void
Mbc1Mapper::reset()
{
  resetRam();
  m_romBankLow = 1;
  m_bankHigh = 0;
  m_bankingMode = false;
}

void
Mbc1Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBankLow);
  writer.writeU8(m_bankHigh);
  writer.writeBool(m_bankingMode);
  serializeRam(writer);
}

void
Mbc1Mapper::deserialize(SaveStateReader& reader)
{
  m_romBankLow = reader.readU8();
  m_bankHigh = reader.readU8();
  m_bankingMode = reader.readBool();
  deserializeRam(reader);
}

}
