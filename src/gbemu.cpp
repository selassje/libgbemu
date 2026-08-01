module gbemu;

namespace {

constexpr std::uint16_t CGB_FLAG_ADDRESS = 0x0143;
// Bit 7 alone ($80) means "supports CGB, but still runs on DMG"; bits 7+6
// together ($C0) mean "CGB required" - real hardware's own boot ROM checks
// these same two bits the same way, it's not an emulator-invented
// distinction.
constexpr std::uint8_t CGB_SUPPORTED_MASK = 0x80;
constexpr std::uint8_t CGB_REQUIRED_MASK = 0xC0;

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
  m_isCgb = (cgbFlag & CGB_SUPPORTED_MASK) != 0;
  const bool cgbRequired = (cgbFlag & CGB_REQUIRED_MASK) == CGB_REQUIRED_MASK;

  if (m_model == Mode::Dmg && cgbRequired) {
    result = std::unexpected(
      "cartridge requires CGB hardware (header byte 0x0143), cannot force "
      "Mode::Dmg for it");
    return result;
  }

  // Mode::Auto gives each cartridge the physical console it
  // actually targets (DMG-only carts boot as DMG, CGB-aware/required
  // carts boot as CGB); Mode::Dmg/Cgb force a specific physical
  // console regardless of what the cartridge declares, for deliberately
  // running a cartridge - even a DMG-only one on Cgb, or a CGB-aware one
  // on Dmg - on hardware other than what it targets, matching a real
  // console's own fixed boot ROM (a real DMG or CGB console runs the same
  // boot ROM no matter what's inserted). Some hardware quirks genuinely
  // differ between the two physical consoles even in compatibility mode
  // (e.g. APU behavior on power-on - see Apu::setCgbMode()), so this is a
  // real behavioral choice, not just which boot animation plays.
  const bool bootAsCgb =
    m_model == Mode::Cgb || (m_model == Mode::Auto && m_isCgb);
  m_mmu.enableBootRom(bootAsCgb ? cgbBootRom() : dmgBootRom());
  m_apu.setCgbMode(bootAsCgb);
  m_mmu.setCgbMode(bootAsCgb);
  m_ppu.setCgbMode(bootAsCgb);
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
  m_cpu = Cpu(m_mmu, m_ppu, m_apu);
  return initializeFromRom();
}

std::expected<EmulationFrame, std::string>
gbemu::GameBoy::runNextFrame()
{
  constexpr std::size_t mCyclesPerFrame = 17556;
  m_apu.startFrame();
  std::size_t mCycles = 0;
  while (mCycles < mCyclesPerFrame) {
    // Cpu::runNextInstruction() ticks Ppu/Mmu/Apu itself now (see
    // Cpu::advanceHardware()), at the specific memory-access points within
    // an instruction that already called it for timer-accuracy reasons,
    // rather than this loop catching everything up in one batch afterward
    // - needed so a mid-instruction Wave RAM read (see
    // Apu::readWaveRam()) observes the channel's state as of its own
    // T-cycle, not whatever was left over from the previous instruction.
    // Not every memory access gets this treatment (e.g. opcode/operand
    // fetches don't), only the ones advanceHardware() was already being
    // called around.
    const auto result = m_cpu.runNextInstruction();
    if (!result) {
      return std::unexpected(result.error());
    }
    mCycles += result.value();
  }
  const auto audioBuffer = m_apu.buffer();
  const EmulationFrame frame = {
    std::mdspan<const std::uint8_t,
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
