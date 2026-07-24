#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <new>

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

std::expected<void, std::string>
runFor(std::chrono::duration<std::size_t, std::milli> duration,
       gbemu::GameBoy& gb)
{
  constexpr std::size_t framesPerSecond = 60;
  constexpr std::size_t millisecondsPerSecond = 1000;
  const auto numberOfFrames =
    (duration.count() * framesPerSecond) / millisecondsPerSecond;
  std::size_t framesRun = 0;
  while (framesRun < numberOfFrames) {
    const auto result = gb.runNextFrame();
    if (!result) {
      return std::unexpected(result.error());
    }
    ++framesRun;
  }
  return {};
}

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

TEST_CASE("06-ld r,r", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "06-ld r,r.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result =
    runFor(std::chrono::milliseconds(1000), // NOLINT(readability-magic-numbers)
           gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("04-op r,imm", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "04-op r,imm.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("03-op sp,hl", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "03-op sp,hl.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("01-special", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "01-special.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("05-op rp", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "05-op rp.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("07-jr,jp,call,ret,rst", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "07-jr,jp,call,ret,rst.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("08-misc instrs", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "08-misc instrs.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("09-op r,r", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "09-op r,r.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("10-bit ops", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "10-bit ops.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("11-op a,(hl)", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "11-op a,(hl).gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("02-interrupts", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "individual" / "02-interrupts.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(20000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}

TEST_CASE("cpu_instrs (combined)", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "cpu_instrs" /
                      "cpu_instrs.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(
    std::chrono::milliseconds(60000), // NOLINT(readability-magic-numbers)
    gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  REQUIRE_THAT(gbemu::gSerialOutput,
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::gSerialOutput.clear();
}
}