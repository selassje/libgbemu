export module gbemu:hardware_mode;

import std;

namespace gbemu {

enum class HardwareMode : std::uint8_t // NOLINT(misc-use-internal-linkage)
{
  Dmg,
  // CGB hardware running a cartridge that doesn't declare CGB support -
  // real hardware's own DMG-compatibility scheme applies.
  CgbCompatibility,
  // CGB hardware running a cartridge that does declare CGB support -
  // native CGB rendering features (VRAM bank 1 tile attributes, 8 BG/8
  // OBJ palettes, CGB priority rules, ...) apply.
  CgbNative,
};

}
