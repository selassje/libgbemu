#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;

namespace {

std::vector<std::uint8_t>
readFile(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  return { std::istreambuf_iterator<char>(file),
           std::istreambuf_iterator<char>() };
}

}

TEST_CASE("GameBoy::create rejects a too-small ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE - 1, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == gbemu::RomLoadError::BadRomSize);
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  std::vector<std::uint8_t> rom(gbemu::MIN_ROM_SIZE, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());
}

TEST_CASE("06-ld r,r", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs"
                       / "individual" / "06-ld r,r.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());
}