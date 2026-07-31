module gbemu;

namespace {

constexpr std::uint16_t CGB_FLAG_ADDRESS = 0x0143;
constexpr std::uint8_t CGB_FLAG_MASK = 0x80;

}

namespace gbemu {

[[nodiscard]] std::expected<void, std::string>
GameBoy::initializeFromRom()
{
  auto result = m_mmu.loadRom(m_romBytes);
  if (!result) {
    return result;
  }

  const auto cgbFlag = m_mmu.readByte(CGB_FLAG_ADDRESS);
  m_isCgb = (cgbFlag & CGB_FLAG_MASK) != 0;
  // Always boot as CGB hardware, matching how a real CGB console always
  // runs its one fixed boot ROM regardless of what's inserted - the boot
  // ROM itself reads this same header flag to decide whether to enter
  // DMG-compatibility mode or native CGB mode for this cartridge, not
  // something selected from outside by which binary we choose to run.
  m_mmu.enableBootRom(cgbBootRom());
  m_cpu.reset();

  return result;
}

[[nodiscard]] std::expected<void, std::string>
GameBoy::loadRom(std::span<const std::uint8_t> rom)
{
  m_romBytes.assign(rom.begin(), rom.end());
  return initializeFromRom();
}

[[nodiscard]] std::expected<void, std::string>
GameBoy::reset()
{
  // Reset first - Mmu's constructor below takes a reference to it.
  m_apu = Apu{};
  // Not m_mmu = Mmu{} - that constructs a ~58KB temporary Mmu on the stack
  // before assigning it in, which trips MSVC /analyze's C6262 (excessive
  // stack usage) treated as an error under /WX. Destroying and
  // reconstructing in-place avoids the temporary entirely.
  m_mmu.~Mmu();
  new (&m_mmu) Mmu(m_apu);
  // Not m_ppu = Ppu(m_mmu) - Ppu's Fetcher members capture *this in their
  // default member initializers, so constructing a temporary Ppu and
  // assigning it in would leave those bound to the temporary's (about to
  // be destroyed) address, not m_ppu's. Destroying and reconstructing
  // in-place ensures *this inside the constructor is the real, persistent
  // m_ppu.
  m_ppu.~Ppu();
  new (&m_ppu) Ppu(m_mmu);
  m_cpu = Cpu(m_mmu, m_ppu);
  return initializeFromRom();
}

std::expected<EmulationFrame, std::string>
gbemu::GameBoy::runNextFrame()
{
  constexpr std::size_t mCyclesPerFrame = 17556;
  m_apu.startFrame();
  std::size_t mCycles = 0;
  while (mCycles < mCyclesPerFrame) {
    const auto result = m_cpu.runNextInstruction();
    if (!result) {
      return std::unexpected(result.error());
    }
    const auto cycles = result.value();
    for (std::size_t i = 0; i < cycles * 4; ++i) {
      m_ppu.runNextTCycle();
      m_mmu.runNextTCycle();
      m_apu.runNextTCycle();
    }
    mCycles += cycles;
  }
  const auto& audioBuffer = m_apu.buffer();
  const EmulationFrame frame = {
    std::mdspan<std::uint8_t,
                std::extents<std::size_t, SCREEN_HEIGHT, SCREEN_WIDTH, 3>>(
      m_ppu.frameBuffer().data(), SCREEN_HEIGHT, SCREEN_WIDTH, 3),
    std::mdspan<const float, std::extents<std::size_t, std::dynamic_extent, 2>>(
      audioBuffer.data(), audioBuffer.size() / 2, 2)
  };

  return { frame };
}

void
GameBoy::setButtonState(Button button, bool pressed)
{
  m_mmu.setButtonState(button, pressed);
}

};
