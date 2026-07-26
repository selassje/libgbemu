export module gbemu:regs;

import std;

namespace gbemu::regs {

export inline constexpr std::uint16_t TIMA = 0xFF05;
export inline constexpr std::uint16_t TMA = 0xFF06;
export inline constexpr std::uint16_t TAC = 0xFF07;
export inline constexpr std::uint16_t IF = 0xFF0F;
export inline constexpr std::uint16_t LCDC = 0xFF40;
export inline constexpr std::uint16_t LY = 0xFF44;
export inline constexpr std::uint16_t BOOT_ROM_DISABLE = 0xFF50;
export inline constexpr std::uint16_t IE = 0xFFFF;

}
