# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

A Game Boy (DMG/CGB) emulator core library written in C++23 using named modules
(`.cppm`), built with CMake + CMake Presets and Conan for dependencies (only
Catch2 currently). Test correctness is validated against blargg's hardware
test ROMs (a git submodule), not hand-written unit assertions of behavior.

## Build / test commands

First-time setup requires the test ROMs submodule:

```
git submodule update --init --recursive
```

Building is preset-driven; there is no single "the" build — pick one from
`CMakePresets.json` (`dev_ninja_gcc`, `dev_ninja_clang_linux`,
`dev_ninja_clang_tidy_linux`, `dev_ninja_clang_coverage_linux`,
`dev_ninja_msvc`, `release_ninja_gcc`, `release_ninja_clang_linux`, etc.).
Conan bootstraps itself automatically on first `cmake --preset` if the
toolchain file for that preset doesn't exist yet under `builds/<preset>/` —
no separate manual `conan install` step needed.

```
cmake --preset dev_ninja_gcc
cmake --build --preset dev_ninja_gcc
ctest --preset dev_ninja_gcc
```

Or build/run the test binary directly (finer control, e.g. filtering to one
`TEST_CASE` by name via Catch2's own CLI):

```
cd builds/dev_ninja_gcc/build
ninja gbemu-tests
./tests/gbemu-tests "06-ld r,r"        # run a single named test case
./tests/gbemu-tests --durations yes    # see per-test timing
```

Formatting (there are dedicated build targets, not a separate script):

```
ninja clang-format          # or clang-format-check for a dry-run/CI-style check
ninja cmake-format          # or cmake-format-check
```

Preset naming encodes what's enabled: `_clang_tidy_` = `ENABLE_CLANG_TIDY=ON`
(static analysis errors are build errors, not a separate lint pass),
`_coverage_` = `ENABLE_COVERAGE_REPORT=ON` (adds a `generate-coverage-report`
target), `_linux`/no suffix = libc++ vs. platform-default stdlib, `dev_` vs.
`release_` = `CMAKE_BUILD_TYPE`. `dev_ninja_msvc_analysis` enables MSVC's
`/analyze`. When fixing a bug, it's worth checking whether it reproduces
identically on both a GCC and a Clang preset — the two toolchains have caught
genuinely different classes of mistakes here before (narrowing conversions,
useless-cast, unused-variable-under-`-Werror` in release-only optimization
levels).

## Architecture

**Module structure**: `gbemu` is the primary module interface unit
(`import/gbemu.cppm`), which re-exports four partitions —
`:cpu`, `:mmu`, `:ppu`, `:boot_rom` — each living in `src/<name>.cppm`
(interface: class declarations, exported free functions) paired with
`src/<name>.cpp` (implementation, `module gbemu;` + `namespace gbemu { ... }`,
no partition name repeated). `import/gbemu.cppm` itself also defines the
top-level `GameBoy` facade class — the only type consumers construct
directly. Adding a new source file means registering it in **both**
`src/CMakeLists.txt`'s `CXX_MODULES` file set (interface) and its adjacent
`PRIVATE` sources list (implementation) — CMake's C++ module dependency
scanning does not discover partitions on its own.

**Component wiring**: `GameBoy` owns `Mmu`, then `Ppu` (constructed with a
`Mmu&`), then `Cpu` (constructed with `Mmu&` + `Ppu&`) — declaration order in
the class matters here since member init order follows it and `Cpu`/`Ppu`
hold references into `Mmu`, not copies.

**Boot ROM is genuinely executed, not skipped.** `GameBoy::loadRom()` reads
the cartridge header's CGB flag (`0x0143`) to pick `dmgBootRom()` or
`cgbBootRom()` (clean-room reimplementations from the SameBoy project, MIT
licensed — see attribution in `src/boot_rom.cpp` — not Nintendo's copyrighted
binary), maps it in via `Mmu::enableBootRom()`, and resets `Cpu` to true
hardware power-on state (`PC=0`, `SP=0xFFFF`, everything else zero). The CPU
reaches its real post-boot register values by actually running the boot ROM,
the same as real hardware — there is no "seed post-boot values directly"
shortcut. The boot ROM unmaps itself via a write to `0xFF50`, which is a
one-way latch (`Mmu`): once disabled it cannot be re-enabled by writing 0
again, only a fresh `Mmu` (power cycle) undoes it.

**Opcode dispatch is one handler per instruction *family*, not per opcode.**
`Cpu::INSTRUCTIONS` is a 256-entry `constexpr` table of member-function
pointers built once from ranges/strides (e.g. `ldRR` alone covers all 63
`LD r,r'` opcodes). Handlers take no opcode parameter; each decodes whatever
bit-fields it needs from `m_currentOpcode`, a single opcode byte cached once
per instruction in `runNextInstruction()` *before* any PC adjustment — this
separation (identify the opcode once, up front, independently of where `PC`
ends up) is specifically what makes the HALT-bug emulation possible (see
below) without every handler needing special-casing.

**Cycle-accurate quirks worth knowing before touching `Cpu`/timer/interrupt
code:**
- `m_mcycles` is a running total; `handleTimer()` takes an explicit
  `currentMCycles` parameter and is called *both* once per instruction and
  again mid-instruction (at each memory access inside a handler) so
  timer/memory-access-cycle-timing test ROMs see ticks at the exact right
  M-cycle, not just at instruction boundaries.
- The DMG **HALT bug** (`HALT` with `IME=0` and an interrupt already
  pending) is modeled: the opcode fetch immediately after the buggy `HALT`
  decodes correctly (via the cached `m_currentOpcode`), but `PC` is
  decremented by one *before* the handler runs, so the handler's *operand*
  reads (which use `PC` directly) land one byte early, reproducing the real
  hardware's operand-corruption + one-byte-early continuation.
- `EI`'s interrupt-enable takes effect one instruction later
  (`m_imeEnableDelay`), not immediately.
- Interrupt-pending checks mask `IE & IF` to the real 5 bits
  (bits 5-7 are unused); `Mmu::readByte(0xFF0F)` reports those unused bits
  back as `1`, matching real `IF` hardware behavior.

**Test-ROM result capture.** Blargg ROMs report pass/fail one of two ways
depending on which generation of his test shell they were built with, and
`tests/gameboy_test.cpp` captures both, gated behind `ENABLE_TESTS` (which
also reaches library code, not just the `tests/` target, via
`setup_tests_flags()`):
- Older shells write the result character-by-character to the serial port
  (`SB`/`SC` at `0xFF01`/`0xFF02`, triggered by writing `$81` to `SC`) —
  captured into `gbemu::gSerialOutput` (instant-complete: the byte is
  captured and the transfer flag cleared immediately, no real ~4096-T-cycle
  transfer delay is modeled).
- Newer shells instead write a status byte + zero-terminated string directly
  into cartridge RAM at `$A000`/`$A004` — captured into
  `gbemu::gMemoryOutput`, written *positionally* (indexed by address, not
  appended in write-order) since the shell's print routine interleaves
  null-terminator writes with the actual characters.

  Check whichever channel a given ROM actually uses before writing a new
  `TEST_CASE` — grepping the ROM's own `source/common/shell.s` for
  `final_result`/`text_out_base` (memory channel) vs. plain `SB`/`SC` writes
  (serial) tells you which, if source is available; otherwise just check
  both.

## Monitoring CI

`gh` (GitHub CLI) is available and authenticated. Prefer it over raw `curl`
against the GitHub API for anything beyond a quick unauthenticated read,
since log downloads require auth even on this public repo.

```
gh run list --branch main --limit 5
gh run view <run-id>                # job/step summary
gh run view <run-id> --log-failed   # full logs of only the failed steps
```

If a run is still `in_progress`, don't block synchronously waiting on it —
poll in a backgrounded shell call instead:

```
until gh run view <run-id> --json status -q .status | grep -q completed; do sleep 20; done
```

The public GitHub REST API (`curl -s https://api.github.com/repos/<owner>/<repo>/actions/runs...`)
also works without any authentication for run/job status on this public repo,
useful as a fallback if `gh` isn't installed — but job log downloads
(`/actions/jobs/<id>/logs`) return a 403 ("Must have admin rights") without
auth, so use `gh run view --log-failed` for actual log content.
