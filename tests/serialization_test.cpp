#include <catch2/catch_test_macros.hpp>

import std;
import gbemu;
import test_helpers;

TEST_CASE("SaveStateWriter/SaveStateReader round-trip every field type",
          "[Serialization]")
{
  gbemu::SaveStateWriter writer;
  writer.writeU8(0x12);
  writer.writeU16(0x3456);
  writer.writeU32(0x789ABCDE);
  writer.writeBool(true);
  writer.writeBool(false);
  const std::array<std::uint8_t, 3> sourceBytes{ 0xAA, 0xBB, 0xCC };
  writer.writeBytes(sourceBytes);

  gbemu::SaveStateReader reader{ writer.bytes() };
  REQUIRE(reader.readU8() == 0x12);
  REQUIRE(reader.readU16() == 0x3456);
  REQUIRE(reader.readU32() == 0x789ABCDE);
  REQUIRE(reader.readBool() == true);
  REQUIRE(reader.readBool() == false);
  std::array<std::uint8_t, 3> readBytes{};
  reader.readBytes(readBytes);
  REQUIRE(readBytes == sourceBytes);
}

TEST_CASE("SaveStateWriter encodes multi-byte values little-endian",
          "[Serialization]")
{
  gbemu::SaveStateWriter writer;
  writer.writeU16(0x3456);
  writer.writeU32(0x789ABCDE);

  const auto& bytes = writer.bytes();
  REQUIRE(bytes.size() == 6);
  REQUIRE(bytes.at(0) == 0x56);
  REQUIRE(bytes.at(1) == 0x34);
  REQUIRE(bytes.at(2) == 0xDE);
  REQUIRE(bytes.at(3) == 0xBC);
  REQUIRE(bytes.at(4) == 0x9A);
  REQUIRE(bytes.at(5) == 0x78);
}

TEST_CASE("SaveStateReader throws on a truncated buffer", "[Serialization]")
{
  const std::array<std::uint8_t, 1> tooShort{ 0x42 };
  gbemu::SaveStateReader reader{ tooShort };

  REQUIRE_THROWS_AS(reader.readU16(), std::out_of_range);
}

TEST_CASE("GameBoy::saveState()/loadState() resume exactly where they left "
          "off",
          "[GameBoy][Serialization]")
{
  gbemu::serialOutput().clear();

  // A real, actively-running ROM (not a static test image like dmg-acid2,
  // which converges to an unchanging frame and so wouldn't actually
  // exercise whether mid-execution Cpu/Ppu/Apu state round-tripped) -
  // "06-ld r,r" is one of the fastest individual cpu_instrs ROMs (see
  // cpu_test.cpp's own GB_SERIAL_ROM_TEST for it), chosen here purely for
  // real, varied CPU/timer/PPU/APU activity to diverge on if save/load
  // lost anything, not for its own pass/fail result.
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) /
                      "cpu_instrs/individual/06-ld r,r.gb");

  gbemu::GameBoy reference{};
  REQUIRE(reference.loadRom(rom).has_value());
  constexpr int framesBeforeSave = 30;
  for (int i = 0; i < framesBeforeSave; ++i) {
    REQUIRE(reference.runNextFrame().has_value());
  }

  const auto savedState = reference.saveState();

  constexpr int framesAfterSave = 10;
  for (int i = 0; i < framesAfterSave - 1; ++i) {
    REQUIRE(reference.runNextFrame().has_value());
  }
  // EmulationFrame's pixels/audio are views into GameBoy's own internal
  // buffers (see gbemu.cppm), not an owned copy - captured into owned
  // vectors immediately so later calls on either GameBoy don't invalidate
  // what's being compared below.
  const auto referenceFrame = reference.runNextFrame();
  REQUIRE(referenceFrame.has_value());
  const std::span<const std::uint8_t> referencePixelSpan(
    referenceFrame->pixels.data_handle(), referenceFrame->pixels.size());
  const std::vector<std::uint8_t> referencePixels(referencePixelSpan.begin(),
                                                  referencePixelSpan.end());

  gbemu::GameBoy resumed{};
  REQUIRE(resumed.loadRom(rom).has_value());
  REQUIRE(resumed.loadState(savedState).has_value());
  for (int i = 0; i < framesAfterSave - 1; ++i) {
    REQUIRE(resumed.runNextFrame().has_value());
  }
  const auto resumedFrame = resumed.runNextFrame();
  REQUIRE(resumedFrame.has_value());

  REQUIRE(referencePixels.size() == resumedFrame->pixels.size());
  REQUIRE(std::equal(referencePixels.begin(),
                     referencePixels.end(),
                     resumedFrame->pixels.data_handle()));

  gbemu::serialOutput().clear();
}

TEST_CASE("GameBoy::loadState() rejects a bad magic or version mismatch",
          "[GameBoy][Serialization]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) /
                      "cpu_instrs/individual/06-ld r,r.gb");
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  const std::array<std::uint8_t, 8> wrongMagic{
    'N', 'O', 'P', 'E', 0, 0, 0, 0
  };
  const auto badMagicResult = gb.loadState(wrongMagic);
  REQUIRE_FALSE(badMagicResult.has_value());

  gbemu::SaveStateWriter writer;
  const std::array<std::uint8_t, 4> realMagic{ 'G', 'B', 'S', 'T' };
  writer.writeBytes(realMagic);
  writer.writeU32(0xFFFFFFFF);
  const auto badVersionResult = gb.loadState(writer.bytes());
  REQUIRE_FALSE(badVersionResult.has_value());
}

TEST_CASE("GameBoy::loadState() leaves state untouched when the body is "
          "truncated",
          "[GameBoy][Serialization]")
{
  gbemu::serialOutput().clear();

  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) /
                      "cpu_instrs/individual/06-ld r,r.gb");

  gbemu::GameBoy reference{};
  REQUIRE(reference.loadRom(rom).has_value());
  gbemu::GameBoy gb{};
  REQUIRE(gb.loadRom(rom).has_value());

  constexpr int framesBeforeTruncatedLoad = 30;
  for (int i = 0; i < framesBeforeTruncatedLoad; ++i) {
    REQUIRE(reference.runNextFrame().has_value());
    REQUIRE(gb.runNextFrame().has_value());
  }

  // Valid magic/version, but cut off partway through the component data -
  // passes the header check (which never touches component state) and
  // only fails once deserializeComponents() is already mutating gb, the
  // exact case the pre-load snapshot/restore in loadState() exists for.
  const auto validState = gb.saveState();
  const auto truncatedLength =
    static_cast<std::ptrdiff_t>(validState.size() / 2);
  const std::vector<std::uint8_t> truncatedState(
    validState.begin(), validState.begin() + truncatedLength);
  const auto result = gb.loadState(truncatedState);
  REQUIRE_FALSE(result.has_value());

  constexpr int framesAfterTruncatedLoad = 10;
  for (int i = 0; i < framesAfterTruncatedLoad - 1; ++i) {
    REQUIRE(reference.runNextFrame().has_value());
    REQUIRE(gb.runNextFrame().has_value());
  }
  // See the round-trip test above on why these are copied out immediately
  // rather than compared as live EmulationFrame views.
  const auto referenceFrame = reference.runNextFrame();
  REQUIRE(referenceFrame.has_value());
  const std::span<const std::uint8_t> referencePixelSpan(
    referenceFrame->pixels.data_handle(), referenceFrame->pixels.size());
  const std::vector<std::uint8_t> referencePixels(referencePixelSpan.begin(),
                                                  referencePixelSpan.end());

  const auto gbFrame = gb.runNextFrame();
  REQUIRE(gbFrame.has_value());

  REQUIRE(referencePixels.size() == gbFrame->pixels.size());
  REQUIRE(std::equal(referencePixels.begin(),
                     referencePixels.end(),
                     gbFrame->pixels.data_handle()));

  gbemu::serialOutput().clear();
}
