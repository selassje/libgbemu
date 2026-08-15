#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import png;
import test_helpers;

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
