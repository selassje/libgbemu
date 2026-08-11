# libgbemu

[![libgbemu CI](https://github.com/selassje/libgbemu/actions/workflows/ci.yml/badge.svg)](https://github.com/selassje/libgbemu/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A Game Boy (DMG) and Game Boy Color (CGB) emulator core, written in modern
C++23 using named modules (`.cppm`). It's a library, not an application - it
has no window, no input handling, and no audio output of its own. Those live
in [gbemu](https://github.com/selassje/gbemu), the SDL3-based frontend built
on top of it.

## Features

- Cycle-accurate CPU (LR35902/SM83), PPU, and APU, including known hardware
  quirks such as the HALT bug and delayed `EI`/timer edge cases.
- Real DMG and CGB boot ROMs are actually executed on startup (clean-room
  reimplementations from the [SameBoy](https://github.com/LIJI32/SameBoy)
  project, not Nintendo's copyrighted binaries) - register state after boot
  comes from running the boot sequence, not a hardcoded "post-boot" snapshot.
- Game Boy Color support: native mode, DMG-compatibility mode (including
  boot-palette colorization of DMG-only games), and CGB double-speed mode
  (KEY1).
- All four APU channels (two pulse, wave, noise), sample-accurate.
- ROM-only, MBC1, MBC3, and MBC5 cartridges: real ROM and RAM banking (a
  bank-select value beyond what a cartridge's own header actually declares
  aliases back onto the bank(s) it does have, matching real hardware) and
  RAM-enable gating (disabled RAM reads back as 0xFF, writes are ignored).
  MBC3 also implements its real-time clock register set, including its own
  range/rollover and sub-second-write quirks. No battery-backed save
  persistence across sessions yet - `saveState()`/`loadState()` round-trip
  an in-memory snapshot, not a `.sav` file.
- No test target exists that skips correctness checks: every behavior claim
  above is backed by a passing hardware test ROM (see Testing).

## Known limitations

- MBC2, MBC6, MBC7, and HuC1/HuC3 aren't implemented; only ROM-only, MBC1,
  MBC3, and MBC5 are (see Features).
- CGB HDMA (general-purpose and HBlank VRAM-to-VRAM DMA, registers
  0xFF51-0xFF55) isn't implemented - writes to $FF55 are stored as a plain
  register with no actual VRAM transfer taking place. Confirmed to cause
  real, visible corruption: The Legend of Zelda: Oracle of Ages' title-screen
  intro issues a General-Purpose DMA (mode bit clear) to populate a window
  tilemap row shortly before displaying it; since the transfer never
  happens, that row keeps whatever was already in VRAM (typically zeroed),
  rendering as a solid color instead of the intended tile art. `c-sp/
  game-boy-test-roms` has extensive coverage for this once it's
  implemented - see `same-suite/dma/` (gdma_addr_mask, hdma_lcd_off,
  hdma_mode0) and `mealybug-tearoom-tests/dma/` (hdma_during_halt,
  hdma_timing) for fundamental correctness, and `gambatte/dma/` for a much
  larger set of cycle-timing edge cases (HALT/double-speed-transition/
  interrupt-precedence interactions during a transfer) beyond what's
  needed for basic correctness.
- No battery-backed save persistence (see Features).

## Testing

Correctness is validated against real hardware test ROMs, not hand-written
unit assertions of expected behavior:

- [blargg's test ROMs](https://github.com/retrio/gb-test-roms) (`cpu_instrs`,
  `instr_timing`, `mem_timing`, `mem_timing-2`, `halt_bug`, `interrupt_time`,
  `dmg_sound`, `cgb_sound`).
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) and
  [cgb-acid2](https://github.com/mattcurrie/cgb-acid2), by Matt Currie -
  pixel-exact PPU rendering tests.
- [MBC3 Tester](https://github.com/EricKirschenmann/MBC3-Tester-gb), by Eric
  Kirschenmann, and [rtc3test](https://github.com/aaaaaa123456789/rtc3test),
  by ax6 - MBC3 ROM/RAM banking (including MBC30's full 8-bit bank register)
  and real-time clock correctness, including its range/rollover and
  sub-second-write quirks.
- [Mooneye Test Suite](https://github.com/Gekkio/mooneye-test-suite)'s
  `emulator-only/mbc1/ram_64kb`/`ram_256kb` - MBC1 RAM banking and
  RAM-enable gating, against both an under- and a fully-populated
  cartridge.

All of the above are fetched at configure time, not vendored in git (the
full test-ROM set is ~177MB across 5000+ files) - only a handful of small
reference frame buffers dmg-acid2/cgb-acid2/the MBC1 tests compare against
(no upstream source provides these) are vendored under `tests/`. See
`CLAUDE.md` for details.

```
cmake --preset dev_ninja_gcc
cmake --build --preset dev_ninja_gcc
ctest --preset dev_ninja_gcc
```

See `CLAUDE.md` for the full list of build presets (GCC/Clang/MSVC,
clang-tidy, sanitizers, coverage) and other build details.

## License

MIT - see [LICENSE](LICENSE). This covers libgbemu's own code; vendored test
assets under `tests/` (dmg-acid2, cgb-acid2) carry their own licenses in
their respective directories, and the embedded SameBoy boot ROM
reimplementation's attribution is noted in `src/boot_rom.cpp`.
