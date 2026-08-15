export module test_helpers;

import std;
import gbemu;

export std::vector<std::uint8_t>
readFile(const std::filesystem::path& path);

export std::expected<void, std::string>
runFor(std::chrono::duration<std::size_t, std::milli> duration,
       gbemu::GameBoy& gb);

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

export std::string
sameSuiteFibonacciPass();

export gbemu::EmulationFrame
stabilizeAndGetFrame(gbemu::GameBoy& gb, int framesToStabilize);
