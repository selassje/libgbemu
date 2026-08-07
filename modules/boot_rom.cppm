export module gbemu:boot_rom;

import std;

namespace gbemu {

// Clean-room boot ROM reimplementations by Lior Halphon (LIJI32), from the
// SameBoy project (MIT License). Not Nintendo's copyrighted binary - see
// src/boot_rom.cpp for the full attribution and the embedded data.
export [[nodiscard]] std::span<const std::uint8_t>
dmgBootRom();
export [[nodiscard]] std::span<const std::uint8_t>
cgbBootRom();

}
