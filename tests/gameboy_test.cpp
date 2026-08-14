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

TEST_CASE("GameBoy::create rejects a ROM size that isn't a multiple of the "
          "16KB bank size",
          "[GameBoy]")
{
  // Regression test for a real crash found by fuzzing (see
  // tests/fuzzing.cpp and ROM_BANK_SIZE's own comment): a size that passes
  // the plain MIN_ROM_SIZE floor but isn't a whole number of banks used to
  // be accepted, then let the CPU read straight past the end of the ROM
  // buffer (std::out_of_range, uncaught) the first time it fetched an
  // instruction beyond the buffer's actual length.
  std::vector<std::uint8_t> rom(gbemu::ROM_BANK_SIZE + 1, 0);
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE_FALSE(result.has_value());
}

TEST_CASE("GameBoy::create accepts a minimally-sized ROM", "[GameBoy]")
{
  // One full bank - the smallest size that's both >= MIN_ROM_SIZE and a
  // whole multiple of ROM_BANK_SIZE (see its own comment on why the latter
  // matters).
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

TEST_CASE("GameBoy::setMode() rejects a CGB-required cartridge forced to "
          "Dmg without mutating state",
          "[GameBoy]")
{
  // Regression test for setMode() used to commit m_model before
  // validating it against the loaded cartridge - a rejected mode change
  // left m_model on the incompatible value, which then made a *plain*
  // reset() (e.g. the frontend's Reset menu item) fail too, since
  // reset()/initializeFromRom() reject the exact same combination. Now
  // that GameBoy::reset() is unconditional (see its own comment),
  // setMode() must validate before ever touching m_model, so a rejected
  // call leaves the GameBoy exactly as it was.
  auto cgbRom =
    readFile(std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc");
  gbemu::GameBoy gb{ gbemu::Mode::Auto };
  REQUIRE(gb.loadRom(cgbRom).has_value());

  auto setModeResult = gb.setMode(gbemu::Mode::Dmg);

  REQUIRE_FALSE(setModeResult.has_value());
  REQUIRE(gb.getMode() == gbemu::Mode::Auto);

  // The real regression: this must not crash/assert - reset() re-derives
  // from the same (still Mode::Auto, still compatible) state as before.
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
  // Regression test for a real crash: loadRom() used to always call
  // reset() first (see its own comment), which destroys and
  // reconstructs Apu/Mmu/Ppu/Cpu unconditionally - if the *new* ROM
  // then failed to load (too small, unsupported cartridge type), that
  // destruction had already happened irreversibly, leaving m_mapper
  // holding no ROM data at all. The very next runNextFrame() call then
  // crashed (std::out_of_range from Mapper::romByte(), fetching an
  // instruction from an empty ROM) - exactly what the frontend's "Open
  // ROM" menu action hit in practice, reusing an already-running
  // GameBoy instance rather than constructing a fresh one.
  auto goodRom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) /
                          "cpu_instrs/individual/06-ld r,r.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(goodRom).has_value());

  constexpr int framesBeforeBadLoad = 10;
  for (int i = 0; i < framesBeforeBadLoad; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }

  // A well-formed header otherwise, but with a cartridge type (0x147)
  // Mmu::loadRom() doesn't recognize. Bank-aligned (ROM_BANK_SIZE, not
  // MIN_ROM_SIZE) so this is rejected for its cartridge type as intended,
  // not short-circuited by the separate bank-alignment check first.
  std::vector<std::uint8_t> badRom(gbemu::ROM_BANK_SIZE, 0);
  constexpr std::size_t cartridgeTypeAddress = 0x147;
  badRom.at(cartridgeTypeAddress) = 0xFF;
  REQUIRE_FALSE(gb.loadRom(badRom).has_value());

  // The crash reproduction: this must not throw - the rejected load
  // above should have left the original game running untouched.
  constexpr int framesAfterBadLoad = 10;
  for (int i = 0; i < framesAfterBadLoad; ++i) {
    REQUIRE(gb.runNextFrame().has_value());
  }
}
