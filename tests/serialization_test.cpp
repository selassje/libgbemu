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
}
