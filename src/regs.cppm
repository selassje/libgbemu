export module gbemu:regs;

import std;

namespace gbemu::regs {

export constexpr std::uint16_t TIMA = 0xFF05;
export constexpr std::uint16_t TMA = 0xFF06;
export constexpr std::uint16_t TAC = 0xFF07;
export constexpr std::uint16_t IF = 0xFF0F;
export constexpr std::uint16_t LCDC = 0xFF40;
export constexpr std::uint16_t BGP = 0xFF47;
export constexpr std::uint16_t LY = 0xFF44;
export constexpr std::uint16_t BOOT_ROM_DISABLE = 0xFF50;
export constexpr std::uint16_t IE = 0xFFFF;
export constexpr std::uint16_t SCX = 0xFF43;
export constexpr std::uint16_t SCY = 0xFF42;
export constexpr std::uint16_t WX = 0xFF4B;
export constexpr std::uint16_t WY = 0xFF4A;

}
