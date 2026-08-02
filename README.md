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
- ROM-only and MBC1 cartridges (ROM banking only - no external RAM/battery
  saves yet).
- No test target exists that skips correctness checks: every behavior claim
  above is backed by a passing hardware test ROM (see Testing).

## Testing

Correctness is validated against real hardware test ROMs, not hand-written
unit assertions of expected behavior:

- [blargg's test ROMs](https://github.com/retrio/gb-test-roms) (`cpu_instrs`,
  `instr_timing`, `mem_timing`, `mem_timing-2`, `halt_bug`, `interrupt_time`,
  `dmg_sound`) - vendored as a git submodule.
- [dmg-acid2](https://github.com/mattcurrie/dmg-acid2) and
  [cgb-acid2](https://github.com/mattcurrie/cgb-acid2), by Matt Currie -
  pixel-exact PPU rendering tests, vendored directly under `tests/`.

```
git submodule update --init --recursive
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
