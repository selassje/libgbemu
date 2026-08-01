export module gbemu:hardware_mode;

import std;

namespace gbemu {

// The actual, resolved hardware behavior this session runs as - distinct
// from the caller-requested Mode (see import/gbemu.cppm): Mode is what the
// caller asked for (Auto/Dmg/Cgb), HardwareMode is what that resolves to
// once the cartridge's own header CGB flag is taken into account (Auto in
// particular can resolve to either Dmg or one of the two Cgb variants here,
// entirely depending on the cartridge - see GameBoy::initializeFromRom()).
// Computed once per load/reset and handed to every subsystem whose
// behavior depends on it via its own setHardwareMode(). Deliberately not
// marked `export` itself (unlike this partition's own `export module`,
// needed structurally so other interface partitions - Apu/Mmu/Ppu - can
// import it at all): this keeps HardwareMode visible everywhere inside
// the gbemu module but invisible to external consumers, the same pattern
// Apu/Mmu/Ppu/Cpu's own class declarations already use for the same
// reason.
enum class HardwareMode : std::uint8_t // NOLINT(misc-use-internal-linkage)
{
  // Genuine DMG hardware - either Mode::Dmg was requested, or Mode::Auto
  // resolved to it because the cartridge doesn't declare CGB support.
  Dmg,
  // CGB hardware running a cartridge that doesn't declare CGB support -
  // real hardware's own DMG-compatibility scheme applies (see
  // Ppu::setHardwareMode()'s comment on what specifically differs from
  // CgbNative).
  CgbCompatibility,
  // CGB hardware running a cartridge that does declare CGB support -
  // native CGB rendering features (VRAM bank 1 tile attributes, 8 BG/8
  // OBJ palettes, CGB priority rules, ...) apply.
  CgbNative,
};

}
