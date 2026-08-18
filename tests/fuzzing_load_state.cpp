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
    gbemu::SAVE_STATE_MAGIC.at(0),
    gbemu::SAVE_STATE_MAGIC.at(1),
    gbemu::SAVE_STATE_MAGIC.at(2),
    gbemu::SAVE_STATE_MAGIC.at(3),
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 24U) & 0xFFU),
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 16U) & 0xFFU),
    static_cast<std::uint8_t>((gbemu::SAVE_STATE_VERSION >> 8U) & 0xFFU),
    static_cast<std::uint8_t>(gbemu::SAVE_STATE_VERSION & 0xFFU)
  };

  std::ranges::copy(std::span(data, size), std::back_inserter(input));

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
