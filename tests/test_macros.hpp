#pragma once

#include <catch2/catch_test_macros.hpp>

// Test-generating macros shared across the test-file split (cpu_test.cpp/
// apu_test.cpp/mapper_test.cpp/ppu_test.cpp) - each expands to a full
// TEST_CASE from a handful of values instead of repeating the same
// load/run/assert boilerplate per ROM. A plain header, not a module: a macro
// can't be exported from a module the way test_helpers' actual functions
// (expectSerialPass()/expectMemoryPass()/stabilizeAndGetFrame()/
// pixelsMatchPng(), all used inside the expansions below) can - `export`
// only applies to real language declarations, and importing a module never
// brings along any #define it happens to contain. Relies entirely on
// whichever .cpp file includes this having already `import`ed std/gbemu/
// test_helpers/png itself first - same as any macro whose expansion
// references external names it doesn't declare.
// NOLINTBEGIN(cppcoreguidelines-macro-usage) - a constexpr function can't
// generate a named TEST_CASE; Catch2's own registration mechanism is a
// macro, so there's no non-macro way to do this.

// Blargg-shell ROMs that report pass/fail over the serial port (SB/SC) -
// see expectSerialPass()'s own comment for which ROM shell generation
// writes results this way rather than through cartridge RAM.
#define GB_SERIAL_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectSerialPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }

// Blargg-shell ROMs that report pass/fail via cartridge RAM instead (the
// newer shell generation) - see expectMemoryPass()'s own comment.
#define GB_MEMORY_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectMemoryPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }

// Loads romPath, runs framesToStabilize frames, then compares the final
// frame against referencePngPath - the shape every mbc1/mbc2/mbc5
// mooneye-test-suite TEST_CASE in mapper_test.cpp reduces to. Tests with
// any extra setup (rtc3test's menu navigation) don't fit this shape and
// stay hand-written.
#define GB_ROM_MATCHES_REFERENCE_PNG_TEST(                                     \
  testName, romPath, referencePngPath, framesToStabilize)                      \
  TEST_CASE(testName, "[GameBoy]")                                             \
  {                                                                            \
    auto rom = readFile(romPath);                                              \
    gbemu::GameBoy gb{};                                                       \
    REQUIRE(gb.loadRom(rom).has_value());                                      \
                                                                               \
    const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);            \
                                                                               \
    REQUIRE(pixelsMatchPng(                                                    \
      std::span(frame.pixels.data_handle(), frame.pixels.size()),              \
      gbemu::SCREEN_WIDTH,                                                     \
      gbemu::SCREEN_HEIGHT,                                                    \
      referencePngPath));                                                      \
  }

// Same shape as GB_ROM_MATCHES_REFERENCE_PNG_TEST above, but against a raw
// (width*height*3, 8-bit RGB, row-major, no padding) referenceRgbPath
// instead of a PNG - dmg-acid2/cgb-acid2/mbc3-tester's own reference.rgb/
// reference_dmg.rgb/reference_cgb.rgb files, predating writePixelsAsPng()'s
// PNG approach the mbc1/mbc2/mbc5 tests use. Also takes mode explicitly
// (those three tests each need a specific one - Mode::Auto included,
// rather than defaulting to it implicitly the way GB_ROM_MATCHES_REFERENCE_
// PNG_TEST's mapper tests all do) since forcing Dmg/Cgb explicitly, rather
// than leaving it to what the cartridge header auto-resolves to, is
// exactly what these particular tests are meant to verify.
#define GB_ROM_MATCHES_REFERENCE_RGB_TEST(                                     \
  testName, mode, romPath, referenceRgbPath, framesToStabilize)                \
  TEST_CASE(testName, "[GameBoy]")                                             \
  {                                                                            \
    auto rom = readFile(romPath);                                              \
    auto reference = readFile(referenceRgbPath);                               \
    gbemu::GameBoy gb{ mode };                                                 \
                                                                               \
    auto result = gb.loadRom(rom);                                             \
    REQUIRE(result.has_value());                                               \
                                                                               \
    const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);            \
                                                                               \
    REQUIRE(reference.size() == frame.pixels.size());                          \
    REQUIRE(std::equal(                                                        \
      reference.begin(), reference.end(), frame.pixels.data_handle()));        \
  }
// NOLINTEND(cppcoreguidelines-macro-usage)
