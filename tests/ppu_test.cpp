#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import png;
import test_helpers;

// The ROM's own source schedules a debugger breakpoint 10 frames after
// Main: starts, at which point its animation has settled and it's meant
// to be compared against the reference image - see dmg-acid2.asm's frame
// counter. That count doesn't include the boot ROM sequence (Nintendo
// logo scroll + chime) that runs first, though - empirically, the first
// byte-exact match against the reference happens at frame 24 measured
// from GameBoy::loadRom() (booting as CGB hardware - see
// GameBoy::initializeFromRom() - runs a shorter logo animation than
// DMG's own boot ROM would), and every frame from there through at least
// 200 matches too (the ROM's own animation is fully periodic once Main:
// is running), so 120 gives a comfortable stable margin.
GB_ROM_MATCHES_REFERENCE_RGB_TEST(
  "dmg-acid2",
  gbemu::Mode::Auto,
  std::filesystem::path(DMG_ACID2_DIR) / "dmg-acid2.gb",
  std::filesystem::path(DMG_ACID2_EXPECTED_DIR) / "reference.rgb",
  120)

GB_ROM_MATCHES_REFERENCE_RGB_TEST(
  "cgb-acid2",
  gbemu::Mode::Cgb,
  std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc",
  std::filesystem::path(CGB_ACID2_EXPECTED_DIR) / "reference.rgb",
  120)

// turtle-tests' own shipped reference screenshots - an independently-sourced
// ground truth (same reasoning as mbc3-tester/rtc3test in mapper_test.cpp)
// specifically for WY/WX window-trigger timing, the exact area WX < 7
// handling (see checkForWindow()'s own comment) lives in.
TEST_CASE("window_y_trigger", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(TURTLE_TESTS_DIR) /
                      "window_y_trigger/window_y_trigger.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToStabilize = 600;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(
    pixelsMatchPng(std::span(frame.pixels.data_handle(), frame.pixels.size()),
                   gbemu::SCREEN_WIDTH,
                   gbemu::SCREEN_HEIGHT,
                   std::filesystem::path(TURTLE_TESTS_DIR) /
                     "window_y_trigger/window_y_trigger.png"));
}

// Same as window_y_trigger above, but with WX set off-screen (< 7) -
// specifically exercises the m_windowPixelsToDiscard clipping path.
TEST_CASE("window_y_trigger_wx_offscreen", "[GameBoy]")
{
  auto rom =
    readFile(std::filesystem::path(TURTLE_TESTS_DIR) /
             "window_y_trigger_wx_offscreen/window_y_trigger_wx_offscreen.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToStabilize = 600;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(
    pixelsMatchPng(std::span(frame.pixels.data_handle(), frame.pixels.size()),
                   gbemu::SCREEN_WIDTH,
                   gbemu::SCREEN_HEIGHT,
                   std::filesystem::path(TURTLE_TESTS_DIR) /
                     "window_y_trigger_wx_offscreen/"
                     "window_y_trigger_wx_offscreen.png"));
}
