module gbemu;

namespace {

constexpr std::uint16_t CGB_FLAG_ADDRESS = 0x0143;
// Bit 7 alone ($80) means "supports CGB, but still runs on DMG"; bits 7+6
// together ($C0) mean "CGB required" - real hardware's own boot ROM checks
// these same two bits the same way, it's not an emulator-invented
// distinction.
constexpr std::uint8_t CGB_SUPPORTED_MASK = 0x80;
constexpr std::uint8_t CGB_REQUIRED_MASK = 0xC0;

// Written first, unconditionally, by every saveState() call - lets
// loadState() reject a file that isn't a gbemu save state at all (wrong
// ROM's save, a corrupted download, an unrelated file) with a clear error
// right away, rather than either misreading it as valid or failing with a
// confusing error from deep inside some component's own deserialize().
constexpr std::array<std::uint8_t, 4> SAVE_STATE_MAGIC = { 'G', 'B', 'S', 'T' };
// Bumped whenever the save-state layout changes (a component gains/loses/
// reorders a serialized field) - loadState() requires an exact match, no
// migration or partial-load attempt for an older/newer version. See
// serialization.cppm's own comment on C++26 reflection eventually
// replacing the hand-maintained per-field serialize()/deserialize() calls
// this version number protects against silently misreading.
constexpr std::uint32_t SAVE_STATE_VERSION = 1;

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
  const bool cartSupportsCgb = (cgbFlag & CGB_SUPPORTED_MASK) != 0;
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
  // boot ROM no matter what's inserted).
  const bool bootAsCgb =
    m_model == Mode::Cgb || (m_model == Mode::Auto && cartSupportsCgb);
  if (!bootAsCgb) {
    m_hardwareMode = HardwareMode::Dmg;
  } else if (cartSupportsCgb) {
    m_hardwareMode = HardwareMode::CgbNative;
  } else {
    m_hardwareMode = HardwareMode::CgbCompatibility;
  }
  m_mmu.enableBootRom(bootAsCgb ? cgbBootRom() : dmgBootRom());
  // Some hardware quirks/rendering rules genuinely differ between the two
  // physical consoles, and between compatibility and native mode on CGB -
  // see each subsystem's own setHardwareMode() comment for what it does
  // with this.
  m_apu.setHardwareMode(m_hardwareMode);
  m_mmu.setHardwareMode(m_hardwareMode);
  m_ppu.setHardwareMode(m_hardwareMode);
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
  constexpr std::size_t tCyclesPerFrame = 70224;
  m_apu.startFrame();
  const auto targetBaseTCycles = m_cpu.baseTCycles() + tCyclesPerFrame;
  while (m_cpu.baseTCycles() < targetBaseTCycles) {
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

std::vector<std::uint8_t>
GameBoy::saveState() const
{
  SaveStateWriter writer;
  writer.writeBytes(SAVE_STATE_MAGIC);
  writer.writeU32(SAVE_STATE_VERSION);
  m_cpu.serialize(writer);
  m_mmu.serialize(writer);
  m_ppu.serialize(writer);
  m_apu.serialize(writer);
  return writer.bytes();
}

std::expected<void, std::string>
GameBoy::loadState(std::span<const std::uint8_t> data)
{
  SaveStateReader reader{ data };
  std::array<std::uint8_t, SAVE_STATE_MAGIC.size()> magic{};
  try {
    reader.readBytes(magic);
  } catch (const std::out_of_range&) {
    return std::unexpected("not a gbemu save state (too short)");
  }
  if (magic != SAVE_STATE_MAGIC) {
    return std::unexpected("not a gbemu save state (bad magic)");
  }

  std::uint32_t version{};
  try {
    version = reader.readU32();
  } catch (const std::out_of_range&) {
    return std::unexpected("not a gbemu save state (truncated header)");
  }
  if (version != SAVE_STATE_VERSION) {
    return std::unexpected(
      "save state version mismatch (this build supports version " +
      std::to_string(SAVE_STATE_VERSION) + ", file is version " +
      std::to_string(version) + ")");
  }

  // Magic and version are already verified above, so only a truncated or
  // otherwise corrupt body can throw here - by this point some component's
  // deserialize() may already have mutated its own state before the
  // exception (unlike the magic/version check above, which never touches
  // any component), so a save file that fails here shouldn't be trusted to
  // resume correctly even after this returns an error.
  try {
    m_cpu.deserialize(reader);
    m_mmu.deserialize(reader);
    m_ppu.deserialize(reader);
    m_apu.deserialize(reader);
  } catch (const std::out_of_range&) {
    return std::unexpected("corrupt or truncated save state");
  }
  return {};
}

};
