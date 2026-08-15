#include "test_macros.hpp"
#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import png;
import test_helpers;

// MBC2 RAM-gate register: only the low nibble of a write below 0x4000
// (with address bit 8 clear) matters for enabling cartridge RAM, and only
// the exact value 0x0A enables it.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 bits_ramg",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/bits_ramg.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "bits_ramg_reference_dmg.png",
  600)

// MBC2 ROM-bank register: only the low 4 bits of a write below 0x4000
// with address bit 8 *set* select the ROM bank, with the usual MBC-wide
// bank-0-reads-as-1 quirk.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 bits_romb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/bits_romb.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "bits_romb_reference_dmg.png",
  600)

// MBC2: writes to $4000-$7FFF have no effect.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 bits_unused",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/bits_unused.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) /
    "bits_unused_reference_dmg.png",
  600)

// MBC2 RAM: starts disabled (reads as all-0xFF), writes while disabled
// have no effect, the physical RAM is only 512 bytes so $A000-$BFFF (8KB
// of address space) mirrors it every 0x200 bytes, and the upper nibble of
// every byte always reads back as 1s since the physical chip's RAM is
// genuinely only 4 bits wide.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 ram",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) / "emulator-only/mbc2/ram.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "ram_reference_dmg.png",
  600)

// rom_*.gb names are in kilobits/megabits (rom_512kb.gb is 4 banks/64KB);
// the actual max a real MBC2 chip supports is rom_2Mb.gb's 16 banks/256KB
// - its 4-bit bank register can't address more.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 rom_512kb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/rom_512kb.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "rom_512kb_reference_dmg.png",
  600)

GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 rom_1Mb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/rom_1Mb.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "rom_1Mb_reference_dmg.png",
  600)

GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc2 rom_2Mb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc2/rom_2Mb.gb",
  std::filesystem::path(MBC2_TEST_EXPECTED_DIR) / "rom_2Mb_reference_dmg.png",
  600)

// Tests ROM bank-switch values 0x01-0xFF, exercising MBC30's full 8-bit
// bank register, not just standard MBC3's 7-bit one. The ROM re-runs
// itself forever after "TEST COMPLETE", so this reads a frame from within
// its first stable post-boot cycle.
GB_ROM_MATCHES_REFERENCE_RGB_TEST(
  "mbc3-tester (dmg)",
  gbemu::Mode::Auto,
  std::filesystem::path(MBC3_TESTER_DIR) / "mbc3-tester.gb",
  std::filesystem::path(MBC3_TESTER_EXPECTED_DIR) / "reference_dmg.rgb",
  180)

GB_ROM_MATCHES_REFERENCE_RGB_TEST(
  "mbc3-tester (cgb)",
  gbemu::Mode::Cgb,
  std::filesystem::path(MBC3_TESTER_DIR) / "mbc3-tester.gb",
  std::filesystem::path(MBC3_TESTER_EXPECTED_DIR) / "reference_cgb.rgb",
  240)

// Exercises writes/reads across every RAM bank on a cartridge declaring
// the full 4 RAM banks (0x0149) - doesn't decode its own pass/fail signal,
// so this only guards against a crash, not correctness of what's stored.
TEST_CASE("mbc1_ram_banks doesn't crash", "[GameBoy]")
{
  auto rom =
    readFile(std::filesystem::path(GBMICROTEST_DIR) / "mbc1_ram_banks.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToRun = 300;
  for (int i = 0; i < framesToRun; ++i) {
    const auto result = gb.runNextFrame();
    if (!result) {
      FAIL("Error : " + result.error());
    }
    REQUIRE(result.has_value());
  }
}

// A cartridge declaring only 1 RAM bank (any bank-select value must alias
// back onto that same bank) and one declaring the full 4 banks. RAM is
// disabled at power-on, and disabled RAM must read back as 0xFF, not
// whatever's actually stored.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc1 ram_64kb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc1/ram_64kb.gb",
  std::filesystem::path(MBC1_TEST_EXPECTED_DIR) / "ram_64kb_reference_dmg.png",
  600)

GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc1 ram_256kb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc1/ram_256kb.gb",
  std::filesystem::path(MBC1_TEST_EXPECTED_DIR) / "ram_256kb_reference_dmg.png",
  600)

// rom_*Mb.gb names are in kilobits/megabits, not kilobytes/megabytes -
// rom_512kb.gb is actually a 64KB/4-bank file.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc5 rom_512kb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc5/rom_512kb.gb",
  std::filesystem::path(MBC5_TEST_EXPECTED_DIR) / "rom_512kb_reference_dmg.png",
  600)

// rom_64Mb.gb (actually 8MB/512 banks) specifically needs bank numbers
// past 255, exercising the 9-bit bank-select register's high bit.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc5 rom_64Mb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc5/rom_64Mb.gb",
  std::filesystem::path(MBC5_TEST_EXPECTED_DIR) / "rom_64Mb_reference_dmg.png",
  600)

// rom_32Mb.gb (4MB/256 banks): bank 255 (0xFF) is the largest value
// reachable through the 8-bit low register alone, with the high bit
// staying 0 the entire time.
GB_ROM_MATCHES_REFERENCE_PNG_TEST(
  "mbc5 rom_32Mb",
  std::filesystem::path(MOONEYE_TEST_SUITE_DIR) /
    "emulator-only/mbc5/rom_32Mb.gb",
  std::filesystem::path(MBC5_TEST_EXPECTED_DIR) / "rom_32Mb_reference_dmg.png",
  600)

// rtc3test.gb boots to a menu picking among its three subtests (Basic
// tests/Range tests/Sub-second writes). The A button (not Start) advances
// past the menu.
TEST_CASE("rtc3test basic tests", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(RTC3TEST_DIR) / "rtc3test.gb");
  gbemu::GameBoy gb{ gbemu::Mode::Dmg };
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToReachMenu = 150;
  for (int i = 0; i < framesToReachMenu; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  constexpr int framesToHoldButton = 4;
  gb.setButtonState(gbemu::Button::A, true);
  for (int i = 0; i < framesToHoldButton; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }
  gb.setButtonState(gbemu::Button::A, false);

  constexpr int framesToStabilize = 1500;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(pixelsMatchPng(
    std::span(frame.pixels.data_handle(), frame.pixels.size()),
    gbemu::SCREEN_WIDTH,
    gbemu::SCREEN_HEIGHT,
    std::filesystem::path(RTC3TEST_DIR) / "rtc3test-basic-tests-dmg.png"));
}

// Range tests is the menu's second item. The Down press here is held for
// only a single frame - this menu has no input debouncing, so holding it
// longer overshoots past Range tests onto Sub-second writes instead.
TEST_CASE("rtc3test range tests", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(RTC3TEST_DIR) / "rtc3test.gb");
  gbemu::GameBoy gb{ gbemu::Mode::Dmg };
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToReachMenu = 150;
  for (int i = 0; i < framesToReachMenu; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  gb.setButtonState(gbemu::Button::Down, true);
  REQUIRE(gb.runNextFrame().has_value());
  gb.setButtonState(gbemu::Button::Down, false);

  constexpr int framesToSettleCursor = 20;
  for (int i = 0; i < framesToSettleCursor; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  gb.setButtonState(gbemu::Button::A, true);
  REQUIRE(gb.runNextFrame().has_value());
  gb.setButtonState(gbemu::Button::A, false);

  constexpr int framesToStabilize = 1500;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(pixelsMatchPng(
    std::span(frame.pixels.data_handle(), frame.pixels.size()),
    gbemu::SCREEN_WIDTH,
    gbemu::SCREEN_HEIGHT,
    std::filesystem::path(RTC3TEST_DIR) / "rtc3test-range-tests-dmg.png"));
}

// Sub-second writes is the menu's third item, reached with two separate
// single-frame Down presses rather than one held-longer press.
TEST_CASE("rtc3test sub-second writes", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(RTC3TEST_DIR) / "rtc3test.gb");
  gbemu::GameBoy gb{ gbemu::Mode::Dmg };
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesToReachMenu = 150;
  for (int i = 0; i < framesToReachMenu; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  constexpr int menuStepsToSubSecondWrites = 2;
  constexpr int framesToSettleCursor = 20;
  for (int press = 0; press < menuStepsToSubSecondWrites; ++press) {
    gb.setButtonState(gbemu::Button::Down, true);
    REQUIRE(gb.runNextFrame().has_value());
    gb.setButtonState(gbemu::Button::Down, false);
    for (int i = 0; i < framesToSettleCursor; ++i) {
      REQUIRE(gb.runNextFrame().has_value());
    }
  }

  gb.setButtonState(gbemu::Button::A, true);
  REQUIRE(gb.runNextFrame().has_value());
  gb.setButtonState(gbemu::Button::A, false);

  constexpr int framesToStabilize = 1500;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(
    pixelsMatchPng(std::span(frame.pixels.data_handle(), frame.pixels.size()),
                   gbemu::SCREEN_WIDTH,
                   gbemu::SCREEN_HEIGHT,
                   std::filesystem::path(RTC3TEST_DIR) /
                     "rtc3test-sub-second-writes-dmg.png"));
}
