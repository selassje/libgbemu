#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;

using namespace gbemu;

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE - 1, 0);
  GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == gbemu::RomLoadError::BadRomSize);
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE, 0);
  GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());
}
