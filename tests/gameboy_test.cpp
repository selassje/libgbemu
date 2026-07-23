#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE - 1, 0);

  auto result = gbemu::GameBoy::create(rom);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == gbemu::RomLoadError::BadRomSize);
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE, 0);

  auto result = gbemu::GameBoy::create(rom);

  REQUIRE(result.has_value());
  REQUIRE(result->romSize() == gbemu::MIN_ROM_SIZE);
}
