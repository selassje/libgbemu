import std;
import gbemu;

// Feeds arbitrary fuzzer-mutated bytes to GameBoy::loadRom() as if they were
// a ROM file - the same untrusted-input boundary loadRom()'s own validation
// (too-small buffer, unsupported cartridge type, CGB-required forced to
// Dmg - see gbemu.cpp) already guards against explicitly, and the one a
// real frontend feeds straight from disk (see App::readRomFile() in the
// gbemu frontend repo). Whatever gets past that validation then also drives
// a bounded number of frames, so the Cpu/Ppu/Apu/mapper pipeline itself
// gets exercised against header bytes/ROM contents libFuzzer is free to set
// to anything, not just what a real .gb/.gbc file would ever contain.
//
// No main() defined here - -fsanitize=fuzzer (see setup_fuzzer() in
// cmake/compiler_and_linker_flags.cmake) links in libFuzzer's own driver,
// which provides one and repeatedly calls this function with its own
// mutated inputs. Only built (see tests/CMakeLists.txt) when ENABLE_FUZZING
// is on - not part of the gbemu-tests Catch2 binary/ctest suite at all. The
// name/casing is fixed by libFuzzer's own ABI, not this project's naming
// convention.
extern "C" int
LLVMFuzzerTestOneInput( // NOLINT(readability-identifier-naming)
  const std::uint8_t* data,
  std::size_t size)
{
  gbemu::GameBoy gb;
  if (!gb.loadRom(std::span(data, size)).has_value()) {
    return 0;
  }

  // A fixed, small bound - this isn't trying to reach any particular game
  // state, just to shake out crashes/UB along the way without letting a
  // single accepted-but-pathological ROM (e.g. an infinite HALT loop) hang
  // one fuzzer iteration.
  constexpr int framesToRun = 60;
  for (int i = 0; i < framesToRun; ++i) {
    if (!gb.runNextFrame().has_value()) {
      break;
    }
  }
  return 0;
}
