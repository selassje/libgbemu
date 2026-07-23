export module gbemu;

import std;

export namespace gbemu {

inline constexpr std::size_t MIN_ROM_SIZE = 0x150;

enum class RomLoadError : std::uint8_t
{
  BadRomSize,
};

class GameBoy
{
public:
  static std::expected<GameBoy, RomLoadError> create(
    std::span<const std::uint8_t> rom)
  {
    if (rom.size() < MIN_ROM_SIZE) {
      return std::unexpected(RomLoadError::BadRomSize);
    }
    return GameBoy{ rom };
  }

  [[nodiscard]] std::size_t romSize() const { return m_rom.size(); }

private:
  explicit GameBoy(std::span<const std::uint8_t> rom)
    : m_rom(rom.begin(), rom.end())
  {
  }

  std::vector<std::uint8_t> m_rom;
};

}
