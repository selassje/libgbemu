#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import test_helpers;

// clang-format off
GB_MEMORY_ROM_TEST("dmg_sound 01-registers", "dmg_sound/rom_singles/01-registers.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 02-len ctr", "dmg_sound/rom_singles/02-len ctr.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 03-trigger", "dmg_sound/rom_singles/03-trigger.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 04-sweep", "dmg_sound/rom_singles/04-sweep.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 05-sweep details", "dmg_sound/rom_singles/05-sweep details.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 06-overflow on trigger", "dmg_sound/rom_singles/06-overflow on trigger.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 07-len sweep period sync", "dmg_sound/rom_singles/07-len sweep period sync.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 08-len ctr during power", "dmg_sound/rom_singles/08-len ctr during power.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 11-regs after power", "dmg_sound/rom_singles/11-regs after power.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 09-wave read while on", "dmg_sound/rom_singles/09-wave read while on.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 10-wave trigger while on", "dmg_sound/rom_singles/10-wave trigger while on.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 12-wave write while on", "dmg_sound/rom_singles/12-wave write while on.gb", 20000)
// cgb_sound's ROMs declare CGB support/requirement in their own header
// (0x0143 = 0xC0), so Mode::Auto - which GB_MEMORY_ROM_TEST's default-
// constructed GameBoy already uses - resolves to CGB on its own, same as
// dmg_sound's own ROMs resolving to DMG without needing Mode::Dmg forced.
GB_MEMORY_ROM_TEST("cgb_sound 01-registers", "cgb_sound/rom_singles/01-registers.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 02-len ctr", "cgb_sound/rom_singles/02-len ctr.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 03-trigger", "cgb_sound/rom_singles/03-trigger.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 04-sweep", "cgb_sound/rom_singles/04-sweep.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 05-sweep details", "cgb_sound/rom_singles/05-sweep details.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 06-overflow on trigger", "cgb_sound/rom_singles/06-overflow on trigger.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 07-len sweep period sync", "cgb_sound/rom_singles/07-len sweep period sync.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 08-len ctr during power", "cgb_sound/rom_singles/08-len ctr during power.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 09-wave read while on", "cgb_sound/rom_singles/09-wave read while on.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 10-wave trigger while on", "cgb_sound/rom_singles/10-wave trigger while on.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 11-regs after power", "cgb_sound/rom_singles/11-regs after power.gb", 20000)
GB_MEMORY_ROM_TEST("cgb_sound 12-wave", "cgb_sound/rom_singles/12-wave.gb", 20000)
// clang-format on
