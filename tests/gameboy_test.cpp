#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

import gbemu;

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::kMinRomSize - 1, 0);

  auto result = gbemu::GameBoy::create(rom);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == gbemu::RomLoadError::BadRomSize);
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::kMinRomSize, 0);

  auto result = gbemu::GameBoy::create(rom);

  REQUIRE(result.has_value());
  REQUIRE(result->rom_size() == gbemu::kMinRomSize);
}
