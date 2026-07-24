module gbemu;

namespace gbemu {

[[nodiscard]] std::expected<void, RomLoadError>
GameBoy::loadRom(std::span<const std::uint8_t> rom)

{
  return m_mmu.loadRom(rom);
}

};