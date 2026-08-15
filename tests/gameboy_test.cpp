#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import test_helpers;

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE - 1, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GameBoy::create rejects a ROM size that isn't a multiple of the "
          "16KB bank size",
          "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::ROM_BANK_SIZE + 1, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::ROM_BANK_SIZE, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());
}

TEST_CASE("GameBoy::reset() re-stabilizes to the same image", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(DMG_ACID2_DIR) / "dmg-acid2.gb");
  auto reference =
    readFile(std::filesystem::path(DMG_ACID2_EXPECTED_DIR) / "reference.rgb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  constexpr int framesToStabilize = 120;
  stabilizeAndGetFrame(gb, framesToStabilize);

  gb.reset();

  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(reference.size() == frame.pixels.size());
  REQUIRE(
    std::equal(reference.begin(), reference.end(), frame.pixels.data_handle()));
}

TEST_CASE("GameBoy::setMode() re-stabilizes to the same image", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(DMG_ACID2_DIR) / "dmg-acid2.gb");
  auto reference =
    readFile(std::filesystem::path(DMG_ACID2_EXPECTED_DIR) / "reference.rgb");
  gbemu::GameBoy gb{ gbemu::Mode::Cgb };

  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  constexpr int framesToStabilize = 120;
  stabilizeAndGetFrame(gb, framesToStabilize);

  auto setModeResult = gb.setMode(gbemu::Mode::Dmg);
  REQUIRE(setModeResult.has_value());

  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(reference.size() == frame.pixels.size());
  REQUIRE(
    std::equal(reference.begin(), reference.end(), frame.pixels.data_handle()));
}

TEST_CASE("GameBoy::create rejects a CGB-required cartridge forced to Dmg",
          "[GameBoy]")
{
  // cgb-acid2.gbc's header declares itself CGB-required (0x0143 = 0xC0),
  // not just CGB-aware.
  auto rom = readFile(std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc");
  gbemu::GameBoy gb{ gbemu::Mode::Dmg };

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GameBoy::setMode() rejects a CGB-required cartridge forced to "
          "Dmg without mutating state",
          "[GameBoy]")
{
  auto cgbRom =
    readFile(std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc");
  gbemu::GameBoy gb{ gbemu::Mode::Auto };
  REQUIRE(gb.loadRom(cgbRom).has_value());

  auto setModeResult = gb.setMode(gbemu::Mode::Dmg);

  REQUIRE_FALSE(setModeResult.has_value());
  REQUIRE(gb.getMode() == gbemu::Mode::Auto);

  gb.reset();
  REQUIRE(gb.getMode() == gbemu::Mode::Auto);
}

TEST_CASE("GameBoy::setMode() rejects changing mode before any ROM is loaded",
          "[GameBoy]")
{
  gbemu::GameBoy gb{ gbemu::Mode::Auto };

  auto setModeResult = gb.setMode(gbemu::Mode::Dmg);

  REQUIRE_FALSE(setModeResult.has_value());
  REQUIRE(gb.getMode() == gbemu::Mode::Auto);
}

TEST_CASE("GameBoy::loadRom() rejecting a ROM leaves an already-running "
          "session untouched",
          "[GameBoy]")
{
  auto goodRom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) /
                          "cpu_instrs/individual/06-ld r,r.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(goodRom).has_value());

  constexpr int framesBeforeBadLoad = 10;
  for (int i = 0; i < framesBeforeBadLoad; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  std::vector<std::uint8_t> badRom(gbemu::ROM_BANK_SIZE, 0);
  constexpr std::size_t cartridgeTypeAddress = 0x147;
  badRom.at(cartridgeTypeAddress) = 0xFF;
  REQUIRE_FALSE(gb.loadRom(badRom).has_value());

  constexpr int framesAfterBadLoad = 10;
  for (int i = 0; i < framesAfterBadLoad; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }
}
