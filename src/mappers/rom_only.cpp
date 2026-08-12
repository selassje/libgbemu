module gbemu;

namespace gbemu {

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
RomOnlyMapper::serialize(SaveStateWriter& writer) const
{
  serializeRam(writer);
}

void
RomOnlyMapper::deserialize(SaveStateReader& reader)
{
  deserializeRam(reader);
}

}
