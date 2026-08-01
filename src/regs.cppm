export module gbemu:regs;

import std;

namespace gbemu::regs {

export constexpr std::uint16_t JOYP = 0xFF00;
export constexpr std::uint16_t SB = 0xFF01;
export constexpr std::uint16_t SC = 0xFF02;
export constexpr std::uint16_t DIV = 0xFF04;
export constexpr std::uint16_t TIMA = 0xFF05;
export constexpr std::uint16_t TMA = 0xFF06;
export constexpr std::uint16_t TAC = 0xFF07;
export constexpr std::uint16_t IF = 0xFF0F;
export constexpr std::uint16_t LCDC = 0xFF40;
export constexpr std::uint16_t BGP = 0xFF47;
export constexpr std::uint16_t OBP0 = 0xFF48;
export constexpr std::uint16_t OBP1 = 0xFF49;
export constexpr std::uint16_t LY = 0xFF44;
export constexpr std::uint16_t BOOT_ROM_DISABLE = 0xFF50;
// CGB-only: VRAM bank select (bit 0) - real DMG hardware doesn't have this
// register at all. See Mmu::setCgbMode().
export constexpr std::uint16_t VBK = 0xFF4F;
// CGB-only: WRAM bank select (bits 0-2, bank 0 reads back/behaves as bank
// 1) - real DMG hardware doesn't have this register at all. See
// Mmu::setCgbMode().
export constexpr std::uint16_t SVBK = 0xFF70;
// CGB-only: background/object palette RAM index+auto-increment (BCPS/OCPS)
// and data (BCPD/OCPD) - see Mmu::setCgbMode() and Mmu::bgPaletteColor()/
// objPaletteColor(). Real DMG hardware doesn't have these registers at
// all.
export constexpr std::uint16_t BCPS = 0xFF68;
export constexpr std::uint16_t BCPD = 0xFF69;
export constexpr std::uint16_t OCPS = 0xFF6A;
export constexpr std::uint16_t OCPD = 0xFF6B;
export constexpr std::uint16_t OPRI = 0xFF6C;
export constexpr std::uint16_t IE = 0xFFFF;
export constexpr std::uint16_t SCX = 0xFF43;
export constexpr std::uint16_t SCY = 0xFF42;
export constexpr std::uint16_t WX = 0xFF4B;
export constexpr std::uint16_t WY = 0xFF4A;
export constexpr std::uint16_t STAT = 0xFF41;
export constexpr std::uint16_t LYC = 0xFF45;
export constexpr std::uint16_t OAM_DMA = 0xFF46;
export constexpr std::uint16_t NR10 = 0xFF10;
export constexpr std::uint16_t NR11 = 0xFF11;
export constexpr std::uint16_t NR12 = 0xFF12;
export constexpr std::uint16_t NR13 = 0xFF13;
export constexpr std::uint16_t NR14 = 0xFF14;
export constexpr std::uint16_t NR21 = 0xFF16;
export constexpr std::uint16_t NR22 = 0xFF17;
export constexpr std::uint16_t NR23 = 0xFF18;
export constexpr std::uint16_t NR24 = 0xFF19;
export constexpr std::uint16_t NR30 = 0xFF1A;
export constexpr std::uint16_t NR31 = 0xFF1B;
export constexpr std::uint16_t NR32 = 0xFF1C;
export constexpr std::uint16_t NR33 = 0xFF1D;
export constexpr std::uint16_t NR34 = 0xFF1E;
export constexpr std::uint16_t NR41 = 0xFF20;
export constexpr std::uint16_t NR42 = 0xFF21;
export constexpr std::uint16_t NR43 = 0xFF22;
export constexpr std::uint16_t NR44 = 0xFF23;
export constexpr std::uint16_t NR50 = 0xFF24;
export constexpr std::uint16_t NR51 = 0xFF25;
export constexpr std::uint16_t NR52 = 0xFF26;
// 16 bytes (0xFF30-0xFF3F), channel 3's waveform sample data - unlike the
// registers above, always readable/writable regardless of APU power state
// (see Mmu::writeByte()'s NR52 handling), so it's just a plain storage
// range with no register-specific behavior of its own to name individually.
export constexpr std::uint16_t WAVE_RAM_START = 0xFF30;

}
