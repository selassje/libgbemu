module;

// Catch2's macros belong in the global module fragment (before
// `module test_helpers;` below), not after it - see frontend.cpp's own
// identical comment on why a traditional header needs to land here rather
// than past the module purview declaration.
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

module test_helpers;

import std;
import gbemu;

std::vector<std::uint8_t>
readFile(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  return { std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>() };
}

std::expected<void, std::string>
runFor(std::chrono::duration<std::size_t, std::milli> duration,
       gbemu::GameBoy& gb)
{
  constexpr std::size_t framesPerSecond = 60;
  constexpr std::size_t millisecondsPerSecond = 1000;
  const auto numberOfFrames =
    (duration.count() * framesPerSecond) / millisecondsPerSecond;
  std::size_t framesRun = 0;
  while (framesRun < numberOfFrames) {
    const auto result = gb.runNextFrame();
    if (!result) {
      return std::unexpected(result.error());
    }
    ++framesRun;
  }
  return {};
}

void
loadAndRun(gbemu::GameBoy& gb,
           const std::filesystem::path& romPath,
           std::chrono::milliseconds duration)
{
  auto rom = readFile(romPath);
  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  result = runFor(duration, gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
}

std::string
sameSuiteFibonacciPass()
{
  return {
    static_cast<char>(3),  static_cast<char>(5),  static_cast<char>(8),
    static_cast<char>(13), static_cast<char>(21), static_cast<char>(34)
  };
}

void
expectSerialPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration)
{
  loadAndRun(gb, romPath, duration);
  REQUIRE_THAT(gbemu::serialOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::serialOutput().clear();
}

void
expectMemoryPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration)
{
  loadAndRun(gb, romPath, duration);
  REQUIRE_THAT(gbemu::memoryOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::memoryOutput().clear();
  gbemu::serialOutput().clear();
}

gbemu::EmulationFrame
stabilizeAndGetFrame(gbemu::GameBoy& gb, int framesToStabilize)
{
  for (int i = 0; i < framesToStabilize - 1; ++i) {
    const auto frameResult = gb.runNextFrame();
    if (!frameResult) {
      FAIL("Error : " + frameResult.error());
    }
  }
  const auto frame = gb.runNextFrame();
  if (!frame) {
    FAIL("Error : " + frame.error());
  }
  REQUIRE(frame.has_value());
  return *frame;
}
