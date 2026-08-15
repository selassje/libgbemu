import std;
import gbemu;

// No main() defined here - libFuzzer's own driver (-fsanitize=fuzzer)
// provides one and repeatedly calls this function with mutated inputs. The
// name/casing is fixed by libFuzzer's own ABI.
extern "C" int
LLVMFuzzerTestOneInput( // NOLINT(readability-identifier-naming)
  const std::uint8_t* data,
  std::size_t size)
{
  gbemu::GameBoy gb;
  if (!gb.loadRom(std::span(data, size)).has_value()) {
    return 0;
  }

  constexpr int framesToRun = 60;
  for (int i = 0; i < framesToRun; ++i) {
    if (!gb.runNextFrame().has_value()) {
      break;
    }
  }

  if (!gb.loadState(std::span(data, size)).has_value()) {
    return 0;
  }

  for (int i = 0; i < framesToRun; ++i) {
    if (!gb.runNextFrame().has_value()) {
      break;
    }
  }
  return 0;
}
