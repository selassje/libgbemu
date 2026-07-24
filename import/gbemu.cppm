export module gbemu;

import std;

export import :cpu;
export import :mmu;

export namespace gbemu {

inline constexpr std::size_t MIN_ROM_SIZE = 0x150;

enum class RomLoadError : std::uint8_t
{
  BadRomSize,
};

class GameBoy
{
public:
  GameBoy() = default;
  
  [[nodiscard]] std::expected<void, RomLoadError> loadRom(
    std::span<const std::uint8_t> rom);

private:
  std::vector<std::uint8_t> m_rom;
};

}
