module gbemu;

namespace gbemu {

[[nodiscard]] std::expected<void, std::string>
GameBoy::loadRom(std::span<const std::uint8_t> rom)

{
  return m_mmu.loadRom(rom);
}

};

std::expected<void, std::string>
gbemu::GameBoy::runNextFrame()
{
  constexpr std::size_t mCyclesPerFrame = 17556;
  std::size_t mCycles = 0;
  while (mCycles < mCyclesPerFrame) {
    const auto result = m_cpu.runNextInstruction();
    if (!result) {
      return std::unexpected(result.error());
    }
    const auto cycles = result.value();
    for (std::size_t i = 0; i < cycles * 4; ++i) {
      m_ppu.runNextTCycle();
    }
    mCycles += cycles;
  }
  return {};
}