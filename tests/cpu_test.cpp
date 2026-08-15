#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

import std;
import gbemu;
import png;
import test_helpers;

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
  gbemu::serialOutput().clear();
  gbemu::memoryOutput().clear();

  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "halt_bug.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(std::chrono::milliseconds(50000), gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::serialOutput() + "|" + gbemu::memoryOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::serialOutput().clear();
  gbemu::memoryOutput().clear();
}

// clang-format off
GB_MEMORY_ROM_TEST("mem_timing-2", "mem_timing-2/mem_timing.gb", 20000)
GB_MEMORY_ROM_TEST("interrupt_time", "interrupt_time/interrupt_time.gb", 20000)
// clang-format on

GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "acceptance/instr/daa",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) / "acceptance/instr/daa.gb",
  std::filesystem::path(MOONEYE_ACCEPTANCE_EXPECTED_DIR) /
    "daa_reference_dmg.png",
  600)
