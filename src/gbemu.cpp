module gbemu;

namespace gbemu {

[[nodiscard]] std::expected<void, RomLoadError>
GameBoy::loadRom(std::span<const std::uint8_t> rom)

{
  if (rom.size() < MIN_ROM_SIZE) {
    return std::unexpected(RomLoadError::BadRomSize);
  }
  m_rom.assign(rom.begin(), rom.end());
  return {};
}

};