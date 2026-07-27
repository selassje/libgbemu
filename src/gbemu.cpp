module gbemu;

namespace {

constexpr std::uint16_t CGB_FLAG_ADDRESS = 0x0143;
constexpr std::uint8_t CGB_FLAG_MASK = 0x80;

}

namespace gbemu {

[[nodiscard]] std::expected<void, std::string>
GameBoy::loadRom(std::span<const std::uint8_t> rom)

{
  auto result = m_mmu.loadRom(rom);
  if (!result) {
    return result;
  }

  const auto cgbFlag = m_mmu.readByte(CGB_FLAG_ADDRESS);
  const bool isCgb = (cgbFlag & CGB_FLAG_MASK) != 0;
  m_mmu.enableBootRom(isCgb ? cgbBootRom() : dmgBootRom());
  m_cpu.reset();

  return result;
}

std::expected<EmulationFrame, std::string>
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
  const EmulationFrame frame = {
    std::mdspan<std::uint8_t,
                std::extents<std::size_t, SCREEN_HEIGHT, SCREEN_WIDTH, 3>>(
      m_ppu.frameBuffer().data(), SCREEN_HEIGHT, SCREEN_WIDTH, 3)
  };

  return { frame };
}

};
