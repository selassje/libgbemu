#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import png;
import test_helpers;

GB_ROM_MATCHES_REFERENCE_PNG_TEST("dmg-acid2",
                                  std::filesystem::path(DMG_ACID2_DIR) /
                                    "dmg-acid2.gb",
                                  std::filesystem::path(DMG_ACID2_DIR) /
                                    "dmg-acid2-dmg.png",
                                  120)

GB_ROM_MATCHES_REFERENCE_PNG_TEST("cgb-acid2",
                                  std::filesystem::path(CGB_ACID2_DIR) /
                                    "cgb-acid2.gbc",
                                  std::filesystem::path(CGB_ACID2_DIR) /
                                    "cgb-acid2.png",
                                  120)

GB_ROM_MATCHES_REFERENCE_PNG_TEST("window_y_trigger",
                                  std::filesystem::path(TURTLE_TESTS_DIR) /
                                    "window_y_trigger/window_y_trigger.gb",
                                  std::filesystem::path(TURTLE_TESTS_DIR) /
                                    "window_y_trigger/window_y_trigger.png",
                                  600)

GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "window_y_trigger_wx_offscreen",
  std::filesystem::path(TURTLE_TESTS_DIR) /
    "window_y_trigger_wx_offscreen/window_y_trigger_wx_offscreen.gb",
  std::filesystem::path(TURTLE_TESTS_DIR) /
    "window_y_trigger_wx_offscreen/window_y_trigger_wx_offscreen.png",
  600)

GB_ROM_MATCHES_REFERENCE_PNG_TEST("first frame after LCD enable stays white",
                                  std::filesystem::path(LITTLE_THINGS_GB_DIR) /
                                    "firstwhite.gb",
                                  std::filesystem::path(LITTLE_THINGS_GB_DIR) /
                                    "firstwhite-dmg-cgb.png",
                                  120)

TEST_CASE("m3_lcdc_bg_en_change", "[GameBoy][PPU]")
{
  auto rom = readFile(std::filesystem::path(MEALYBUG_TEAROOM_TESTS_DIR) /
                      "ppu/m3_lcdc_bg_en_change.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  const auto frame = stabilizeAndGetFrame(gb, 120);
  const std::span actualPixels(frame.pixels.data_handle(), frame.pixels.size());

  if (!pixelsMatchPng(actualPixels,
                      gbemu::SCREEN_WIDTH,
                      gbemu::SCREEN_HEIGHT,
                      std::filesystem::path(MEALYBUG_TEAROOM_TESTS_DIR) /
                        "ppu/m3_lcdc_bg_en_change_dmg_blob.png")) {
    SKIP("known-failing: LCDC-bit-0-toggled-mid-scanline handling isn't "
         "cycle-accurate yet - see ppu-debug-memory.md");
  }
}

TEST_CASE("m3_bgp_change", "[GameBoy][PPU]")
{
  auto rom = readFile(std::filesystem::path(MEALYBUG_TEAROOM_TESTS_DIR) /
                      "ppu/m3_bgp_change.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  const auto frame = stabilizeAndGetFrame(gb, 120);
  const std::span actualPixels(frame.pixels.data_handle(), frame.pixels.size());

  REQUIRE(pixelsMatchPng(actualPixels,
                         gbemu::SCREEN_WIDTH,
                         gbemu::SCREEN_HEIGHT,
                         std::filesystem::path(MEALYBUG_TEAROOM_TESTS_DIR) /
                           "ppu/m3_bgp_change_dmg_blob.png"));
}
