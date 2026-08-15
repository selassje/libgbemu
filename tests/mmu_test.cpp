#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import test_helpers;

// Both ROMs below share SameSuite's common report mechanism: no
// "Passed"/"Failed" text - a passing ROM sets B=3, C=5, D=8, E=13, H=21,
// L=34 (a Fibonacci sequence) and sends those same six register values as
// raw bytes over the serial port.

// gbc_dma_cont.gb exercises genuine GDMA (bit 7 clear when writing HDMA5).
TEST_CASE("gbc_dma_cont (SameSuite, pure GDMA)", "[GameBoy]")
{
  gbemu::serialOutput().clear();

  auto rom =
    readFile(std::filesystem::path(SAME_SUITE_DIR) / "dma/gbc_dma_cont.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  auto result = runFor(std::chrono::milliseconds(2000), gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }

  REQUIRE(gbemu::serialOutput() == sameSuiteFibonacciPass());
  gbemu::serialOutput().clear();
}

// gdma_addr_mask.gb requests HDMA (bit 7 set) with the LCD off. Real
// hardware doesn't special-case "LCD off" for HDMA - it just checks
// whether STAT's mode is already 0 (H-Blank) at the moment of the write,
// and disabling the LCD forces STAT to permanently read mode 0 - so one
// block transfers immediately, then the transfer gets stuck (no further
// H-Blank entry ever comes with the LCD staying off), leaving the rest of
// its nominally longer request untouched.
TEST_CASE("gdma_addr_mask (SameSuite, HDMA with LCD off)", "[GameBoy]")
{
  gbemu::serialOutput().clear();

  auto rom =
    readFile(std::filesystem::path(SAME_SUITE_DIR) / "dma/gdma_addr_mask.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  auto result = runFor(std::chrono::milliseconds(2000), gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }

  REQUIRE(gbemu::serialOutput() == sameSuiteFibonacciPass());
  gbemu::serialOutput().clear();
}
