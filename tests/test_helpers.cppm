export module test_helpers;

import std;
import gbemu;

// Shared by every ROM-driving test across the split gameboy_test.cpp/
// cpu_test.cpp/apu_test.cpp/ppu_test.cpp/mapper_test.cpp/mmu_test.cpp/
// serialization_test.cpp files - kept in its own module (rather than each
// file's own now-gone namespace {} of copy-pasted helpers) so there's one
// definition to change, not N. Catch2's REQUIRE/FAIL/REQUIRE_THAT macros
// are used inside test_helpers.cpp's own definitions below, not exposed
// here - callers just see plain functions, same as any other component's
// module interface/implementation split in this codebase.

export std::vector<std::uint8_t>
readFile(const std::filesystem::path& path);

export std::expected<void, std::string>
runFor(std::chrono::duration<std::size_t, std::milli> duration,
       gbemu::GameBoy& gb);

// Loads romPath, runs it for duration, failing (via Catch2) on any frame
// error - the common prefix expectSerialPass()/expectMemoryPass() below
// both need before checking whichever output channel that ROM's shell
// actually reports through (see gbemu::memoryOutput()'s own comment for
// why some use that instead of the serial port).
export void
loadAndRun(gbemu::GameBoy& gb,
           const std::filesystem::path& romPath,
           std::chrono::milliseconds duration);

export void
expectSerialPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration);

export void
expectMemoryPass(gbemu::GameBoy& gb,
                 const std::filesystem::path& romPath,
                 std::chrono::milliseconds duration);

// SameSuite's common pass report (see mmu_test.cpp's own TEST_CASEs for
// the full explanation) - a function rather than a static-duration
// constant so its std::string construction can't trip clang-tidy's
// bugprone-throwing-static-initialization (a global whose constructor
// could throw bad_alloc, uncatchable before main()).
export std::string
sameSuiteFibonacciPass();

// Runs framesToStabilize frames, failing loudly (via Catch2's FAIL(), same
// as every other check in this module) on any frame error, and returns
// the last one for the caller to compare against a reference image.
export gbemu::EmulationFrame
stabilizeAndGetFrame(gbemu::GameBoy& gb, int framesToStabilize);
