#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import test_helpers;

// Core GameBoy API surface - loadRom()'s own accept/reject rules and
// reset()/setMode()'s re-initialization guarantees. Component-specific
// correctness (CPU/APU/PPU/mapper/DMA/serialization) lives in its own
// cpu_test.cpp/apu_test.cpp/ppu_test.cpp/mapper_test.cpp/mmu_test.cpp/
// serialization_test.cpp instead - this file used to hold all of them
// before it grew past ~980 lines; test_helpers.cpp now holds the shared
// readFile()/stabilizeAndGetFrame()/etc. helpers every one of those files
// (this one included) imports instead of each keeping its own copy.

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE - 1, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE, 0);
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

  auto resetResult = gb.reset();
  REQUIRE(resetResult.has_value());

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
  // Starts on Cgb - setMode() switches it to Dmg below, so this exercises
  // an actual model change, not a same-mode no-op reset.
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
  // not just CGB-aware - a real cartridge exercising the same rejection
  // GameBoy::initializeFromRom() already has a dedicated error message
  // for, rather than a synthetic ROM built just to set that byte.
  auto rom = readFile(std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc");
  gbemu::GameBoy gb{ gbemu::Mode::Dmg };

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}
