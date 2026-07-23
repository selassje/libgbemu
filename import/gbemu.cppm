module;

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

export module gbemu;

export namespace gbemu {

inline constexpr std::size_t kMinRomSize = 0x150;

enum class RomLoadError
{
  BadRomSize,
};

class GameBoy
{
public:
  static std::expected<GameBoy, RomLoadError> create(
    std::span<const std::uint8_t> rom)
  {
    if (rom.size() < kMinRomSize) {
      return std::unexpected(RomLoadError::BadRomSize);
    }
    return GameBoy{ rom };
  }

  [[nodiscard]] std::size_t rom_size() const { return m_Rom.size(); }

private:
  explicit GameBoy(std::span<const std::uint8_t> rom)
    : m_Rom(rom.begin(), rom.end())
  {
  }

  std::vector<std::uint8_t> m_Rom;
};

}
