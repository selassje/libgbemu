#include <algorithm>
#include <vector>
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

  std::vector input = {
    gbemu::SAVE_STATE_MAGIC[0],
    gbemu::SAVE_STATE_MAGIC[1],
    gbemu::SAVE_STATE_MAGIC[2],
    gbemu::SAVE_STATE_MAGIC[3],
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 24) & 0xFF),
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 16) & 0xFF),
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 8) & 0xFF),
    static_cast<std::uint8_t>(gbemu::SAVE_STATE_VERSION & 0xFF)
  };

  std::copy(data, data + size, std::back_inserter(input));

  gbemu::GameBoy gb;
  if (!gb.loadState(input).has_value()) {
    return 0;
  }

  constexpr int framesToRun = 60;
  for (int i = 0; i < framesToRun; ++i) {
    if (!gb.runNextFrame().has_value()) {
      break;
    }
  }
  return 0;
}
