module;
#include <array>
export module gbemu:mmu;

import std;

namespace gbemu {

class Mmu // NOLINT(misc-use-internal-linkage)
{
public:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t KB8 = 0x2000;
  static constexpr std::size_t KB4 = 0x1000;

  Mmu() = default;

  [[nodiscard]] std::uint8_t readByte(std::uint16_t address) const;
  void writeByte(std::uint16_t address, std::uint8_t value);

private:
  std::vector<std::uint8_t> m_rom;
  std::array<std::uint8_t, KB16> m_vram{};
  std::array<std::uint8_t, KB8> m_extRam{};
  std::array<std::uint8_t, KB4 * 8> m_wram{};
  std::array<std::uint8_t, 0xA0> m_oam{};  // NOLINT(readability-magic-numbers)
  std::array<std::uint8_t, 0x80> m_io{};   // NOLINT(readability-magic-numbers)
  std::array<std::uint8_t, 0x7F> m_hram{}; // NOLINT(readability-magic-numbers)
  std::size_t m_switchableRomBank{ 1 };
  std::size_t m_switchableVRamBank{ 0 };
  std::size_t m_switchableWRamBank{ 1 };
  std::uint8_t m_InterruptEnableRegister{ 0 };
  static constexpr std::uint8_t M_UNUSABLE{ 0 };

  [[nodiscard]] std::uint8_t& getByteRef(std::uint16_t address);
};

}