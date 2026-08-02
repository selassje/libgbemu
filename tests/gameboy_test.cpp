#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

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

// Shared by every blargg-style ROM test below: load it, run it for the given
// duration, then check whichever output channel that ROM's shell reports
// through (see Mmu.cppm's comment on memoryOutput() for why some use that
// instead of the serial port).
void
loadAndRun(gbemu::GameBoy& gb,
           const std::filesystem::path& romPath,
           std::chrono::milliseconds duration)
{
  auto rom = readFile(romPath);
  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  result = runFor(duration, gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
}

void
expectSerialPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration)
{
  loadAndRun(gb, romPath, duration);
  REQUIRE_THAT(gbemu::serialOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::serialOutput().clear();
}

void
expectMemoryPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration)
{
  loadAndRun(gb, romPath, duration);
  REQUIRE_THAT(gbemu::memoryOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::memoryOutput().clear();
  gbemu::serialOutput().clear();
}

// Runs framesToStabilize frames, failing loudly (via FAIL(), same as every
// other test here) on any frame error, and returns the last one for the
// caller to compare against a reference image.
gbemu::EmulationFrame
stabilizeAndGetFrame(gbemu::GameBoy& gb, int framesToStabilize)
{
  for (int i = 0; i < framesToStabilize - 1; ++i) {
    const auto frameResult = gb.runNextFrame();
    if (!frameResult) {
      FAIL("Error : " + frameResult.error());
    }
  }
  const auto frame = gb.runNextFrame();
  if (!frame) {
    FAIL("Error : " + frame.error());
  }
  REQUIRE(frame.has_value());
  return *frame;
}

}

// Expands to a full TEST_CASE - one line per ROM instead of the ~15-line
// load/run/assert boilerplate repeated for each one. relPath is joined onto
// GB_TEST_ROMS_DIR, same as every one of these tests already did by hand.
// NOLINTBEGIN(cppcoreguidelines-macro-usage) - a constexpr function can't
// generate a named TEST_CASE; Catch2's own registration mechanism is a
// macro, so there's no non-macro way to do this.
#define GB_SERIAL_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectSerialPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }

#define GB_MEMORY_ROM_TEST(name, relPath, ms)                                  \
  TEST_CASE(name, "[GameBoy]")                                                 \
  {                                                                            \
    gbemu::GameBoy gb{};                                                       \
    expectMemoryPass(gb,                                                       \
                     std::filesystem::path(GB_TEST_ROMS_DIR) / (relPath),      \
                     std::chrono::milliseconds(ms));                           \
  }
// NOLINTEND(cppcoreguidelines-macro-usage)

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

// clang-format off
GB_SERIAL_ROM_TEST("06-ld r,r", "cpu_instrs/individual/06-ld r,r.gb", 1000)
GB_SERIAL_ROM_TEST("04-op r,imm", "cpu_instrs/individual/04-op r,imm.gb", 20000)
GB_SERIAL_ROM_TEST("03-op sp,hl", "cpu_instrs/individual/03-op sp,hl.gb", 20000)
GB_SERIAL_ROM_TEST("01-special", "cpu_instrs/individual/01-special.gb", 20000)
GB_SERIAL_ROM_TEST("05-op rp", "cpu_instrs/individual/05-op rp.gb", 20000)
GB_SERIAL_ROM_TEST("07-jr,jp,call,ret,rst", "cpu_instrs/individual/07-jr,jp,call,ret,rst.gb", 20000)
GB_SERIAL_ROM_TEST("08-misc instrs", "cpu_instrs/individual/08-misc instrs.gb", 20000)
GB_SERIAL_ROM_TEST("09-op r,r", "cpu_instrs/individual/09-op r,r.gb", 20000)
GB_SERIAL_ROM_TEST("10-bit ops", "cpu_instrs/individual/10-bit ops.gb", 20000)
GB_SERIAL_ROM_TEST("11-op a,(hl)", "cpu_instrs/individual/11-op a,(hl).gb", 20000)
GB_SERIAL_ROM_TEST("02-interrupts", "cpu_instrs/individual/02-interrupts.gb", 20000)
GB_SERIAL_ROM_TEST("instr_timing", "instr_timing/instr_timing.gb", 20000)
GB_SERIAL_ROM_TEST("mem_timing", "mem_timing/mem_timing.gb", 20000)
GB_SERIAL_ROM_TEST("cpu_instrs (combined)", "cpu_instrs/cpu_instrs.gb", 55000)
// clang-format on

TEST_CASE("halt_bug", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(GB_TEST_ROMS_DIR) / "halt_bug.gb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);

  REQUIRE(result.has_value());

  result = runFor(std::chrono::milliseconds(50000), gb);
  if (!result.has_value()) {
    FAIL("Error : " + result.error());
  }
  REQUIRE(result.has_value());
  // Diagnostic only for now -- just want to see what this ROM actually
  // prints, channel unknown yet.
  REQUIRE_THAT(gbemu::serialOutput() + "|" + gbemu::memoryOutput(),
               Catch::Matchers::ContainsSubstring("Passed"));
  gbemu::serialOutput().clear();
  gbemu::memoryOutput().clear();
}

// clang-format off
GB_MEMORY_ROM_TEST("mem_timing-2", "mem_timing-2/mem_timing.gb", 20000)
GB_MEMORY_ROM_TEST("interrupt_time", "interrupt_time/interrupt_time.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 01-registers", "dmg_sound/rom_singles/01-registers.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 02-len ctr", "dmg_sound/rom_singles/02-len ctr.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 03-trigger", "dmg_sound/rom_singles/03-trigger.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 04-sweep", "dmg_sound/rom_singles/04-sweep.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 05-sweep details", "dmg_sound/rom_singles/05-sweep details.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 06-overflow on trigger", "dmg_sound/rom_singles/06-overflow on trigger.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 07-len sweep period sync", "dmg_sound/rom_singles/07-len sweep period sync.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 08-len ctr during power", "dmg_sound/rom_singles/08-len ctr during power.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 11-regs after power", "dmg_sound/rom_singles/11-regs after power.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 09-wave read while on", "dmg_sound/rom_singles/09-wave read while on.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 10-wave trigger while on", "dmg_sound/rom_singles/10-wave trigger while on.gb", 20000)
GB_MEMORY_ROM_TEST("dmg_sound 12-wave write while on", "dmg_sound/rom_singles/12-wave write while on.gb", 20000)
// clang-format on

TEST_CASE("dmg-acid2", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(DMG_ACID2_DIR) / "dmg-acid2.gb");
  auto reference =
    readFile(std::filesystem::path(DMG_ACID2_DIR) / "reference.rgb");
  gbemu::GameBoy gb{};

  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  // The ROM's own source schedules a debugger breakpoint 10 frames after
  // Main: starts, at which point its animation has settled and it's meant
  // to be compared against the reference image - see dmg-acid2.asm's frame
  // counter. That count doesn't include the boot ROM sequence (Nintendo
  // logo scroll + chime) that runs first, though - empirically, the first
  // byte-exact match against the reference happens at frame 24 measured
  // from GameBoy::loadRom() (booting as CGB hardware - see
  // GameBoy::initializeFromRom() - runs a shorter logo animation than
  // DMG's own boot ROM would), and every frame from there through at least
  // 200 matches too (the ROM's own animation is fully periodic once Main:
  // is running), so 120 gives a comfortable stable margin.
  constexpr int framesToStabilize = 120;
  const auto frame = stabilizeAndGetFrame(gb, framesToStabilize);

  REQUIRE(reference.size() == frame.pixels.size());
  REQUIRE(
    std::equal(reference.begin(), reference.end(), frame.pixels.data_handle()));
}

TEST_CASE("GameBoy::reset() re-stabilizes to the same image", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(DMG_ACID2_DIR) / "dmg-acid2.gb");
  auto reference =
    readFile(std::filesystem::path(DMG_ACID2_DIR) / "reference.rgb");
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

TEST_CASE("cgb-acid2", "[GameBoy]")
{
  auto rom = readFile(std::filesystem::path(CGB_ACID2_DIR) / "cgb-acid2.gbc");
  auto reference =
    readFile(std::filesystem::path(CGB_ACID2_DIR) / "reference.rgb");
  gbemu::GameBoy gb{ gbemu::Mode::Cgb };

  auto result = gb.loadRom(rom);
  REQUIRE(result.has_value());

  constexpr int framesToStabilize = 120;
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
