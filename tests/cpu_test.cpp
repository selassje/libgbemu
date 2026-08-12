#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

import std;
import gbemu;
import test_helpers;

// Expands to a full TEST_CASE - one line per ROM instead of the ~15-line
// load/run/assert boilerplate repeated for each one. relPath is joined onto
// GB_TEST_ROMS_DIR, same as every one of these tests already did by hand.
// NOLINTBEGIN(cppcoreguidelines-macro-usage) - a constexpr function can't
// generate a named TEST_CASE; Catch2's own registration mechanism is a
// macro, so there's no non-macro way to do this. Duplicated (rather than
// shared via a header) in whichever of this split's files actually need
// it - just this one and apu_test.cpp - since a macro can't be exported
// from test_helpers' module the way the functions it expands to are.
#define GB_SERIAL_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectSerialPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }

#define GB_MEMORY_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectMemoryPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }
// NOLINTEND(cppcoreguidelines-macro-usage)

// clang-format off
GB_SERIAL_ROM_TEST("06-ld r,r", "cpu_instrs/individual/06-ld r,r.gb", 1000)
GB_SERIAL_ROM_TEST("04-op r,imm", "cpu_instrs/individual/04-op r,imm.gb", 20000)
GB_SERIAL_ROM_TEST("03-op sp,hl", "cpu_instrs/individual/03-op sp,hl.gb", 20000)
GB_SERIAL_ROM_TEST("01-special", "cpu_instrs/individual/01-special.gb", 20000)
GB_SERIAL_ROM_TEST("05-op rp", "cpu_instrs/individual/05-op rp.gb", 20000)
GB_SERIAL_ROM_TEST("07-jr,jp,call,ret,rst", "cpu_instrs/individual/07-jr,jp,call,ret,rst.gb", 20000)
GB_SERIAL_ROM_TEST("08-misc instrs", "cpu_instrs/individual/08-misc instrs.gb", 20000)
GB_SERIAL_ROM_TEST("09-op r,r", "cpu_instrs/individual/09-op r,r.gb", 20000)
GB_SERIAL_ROM_TEST("10-bit ops", "cpu_instrs/individual/10-bit ops.gb", 20000)
GB_SERIAL_ROM_TEST("11-op a,(hl)", "cpu_instrs/individual/11-op a,(hl).gb", 20000)
GB_SERIAL_ROM_TEST("02-interrupts", "cpu_instrs/individual/02-interrupts.gb", 20000)
GB_SERIAL_ROM_TEST("instr_timing", "instr_timing/instr_timing.gb", 20000)
GB_SERIAL_ROM_TEST("mem_timing", "mem_timing/mem_timing.gb", 20000)
GB_SERIAL_ROM_TEST("cpu_instrs (combined)", "cpu_instrs/cpu_instrs.gb", 55000)
// clang-format on

TEST_CASE("halt_bug", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "halt_bug.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(std::chrono::milliseconds(50000), gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  // Diagnostic only for now -- just want to see what this ROM actually
  // prints, channel unknown yet.
  REQUIRE_THAT(gbemu::serialOutput() + "|" + gbemu::memoryOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::serialOutput().clear();
  gbemu::memoryOutput().clear();
}

// clang-format off
GB_MEMORY_ROM_TEST("mem_timing-2", "mem_timing-2/mem_timing.gb", 20000)
GB_MEMORY_ROM_TEST("interrupt_time", "interrupt_time/interrupt_time.gb", 20000)
// clang-format on
