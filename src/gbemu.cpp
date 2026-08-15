module gbemu;

namespace {

constexpr std::uint16_t CGB_FLAG_ADDRESS = 0x0143;
// Bit 7 alone ($80) means "supports CGB, but still runs on DMG"; bits 7+6
// together ($C0) mean "CGB required" - real hardware's own boot ROM checks
// these same two bits the same way, it's not an emulator-invented
// distinction.
constexpr std::uint8_t CGB_SUPPORTED_MASK = 0x80;
constexpr std::uint8_t CGB_REQUIRED_MASK = 0xC0;

constexpr std::array<std::uint8_t, 4> SAVE_STATE_MAGIC = { 'G', 'B', 'S', 'T' };
constexpr std::uint32_t SAVE_STATE_VERSION = 4;

[[nodiscard]] std::expected<void, std::string>
checkModeCompatible(gbemu::Mode model, std::span<const std::uint8_t> rom)
{
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  const auto cgbFlag = rom[CGB_FLAG_ADDRESS];
  const bool cgbRequired = (cgbFlag & CGB_REQUIRED_MASK) == CGB_REQUIRED_MASK;
  if (model == gbemu::Mode::Dmg && cgbRequired) {
    return std::unexpected(
      "cartridge requires CGB hardware (header byte 0x0143), cannot force "
      "Mode::Dmg for it");
  }
  return {};
}

}

namespace gbemu {

[[nodiscard]] std::expected<void, std::string>
GameBoy::initializeFromRom()
{
  auto result = m_mmu.loadRom(m_romBytes);
  if (!result) {
    return result;
  }

  result = checkModeCompatible(m_model, m_romBytes);
  if (!result) {
    return result;
  }

  const auto cgbFlag = m_mmu.readByte(CGB_FLAG_ADDRESS);
  const bool cartSupportsCgb = (cgbFlag & CGB_SUPPORTED_MASK) != 0;

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
  m_apu.setHardwareMode(m_hardwareMode);
  m_mmu.setHardwareMode(m_hardwareMode);
  m_ppu.setHardwareMode(m_hardwareMode);

  return result;
}

[[nodiscard]] std::expected<void, std::string>
GameBoy::loadRom(std::span<const std::uint8_t> rom)
{
  auto mapperResult = m_mmu.loadRom(rom);
  if (!mapperResult) {
    return mapperResult;
  }
  mapperResult = checkModeCompatible(m_model, rom);
  if (!mapperResult) {
    return mapperResult;
  }

  m_romBytes.assign(rom.begin(), rom.end());
  reset();
  return mapperResult;
}

void
GameBoy::reset()
{
  m_apu = Apu{};
  m_mmu.~Mmu();
  new (&m_mmu) Mmu(m_apu);
  m_ppu.~Ppu();
  new (&m_ppu) Ppu(m_mmu);
  m_cpu.~Cpu();
  new (&m_cpu) Cpu(m_mmu, m_ppu, m_apu);
  hardAssert(initializeFromRom().has_value(),
             "reset() failed after loadRom()/setMode() already validated "
             "compatibility");
}

[[nodiscard]] std::expected<void, std::string>
GameBoy::setMode(Mode mode)
{
  std::expected<void, std::string> result;
  if (m_romBytes.size() < MIN_ROM_SIZE) {
    result = std::unexpected("no ROM loaded, nothing to change the mode for");
    return result;
  }
  result = checkModeCompatible(mode, m_romBytes);
  if (!result) {
    return result;
  }
  m_model = mode;
  reset();
  return result;
}

std::expected<EmulationFrame, std::string>
gbemu::GameBoy::runNextFrame()
{
  // A real Game Boy frame is a fixed 70224 T-cycles.
  constexpr std::size_t tCyclesPerFrame = 70224;
  m_apu.startFrame();
  const auto targetBaseTCycles = m_cpu.baseTCycles() + tCyclesPerFrame;
  while (m_cpu.baseTCycles() < targetBaseTCycles) {
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

void
GameBoy::serializeComponents(SaveStateWriter& writer) const
{
  m_cpu.serialize(writer);
  m_mmu.serialize(writer);
  m_ppu.serialize(writer);
  m_apu.serialize(writer);
}

void
GameBoy::deserializeComponents(SaveStateReader& reader)
{
  m_cpu.deserialize(reader);
  m_mmu.deserialize(reader);
  m_ppu.deserialize(reader);
  m_apu.deserialize(reader);
}

std::vector<std::uint8_t>
GameBoy::saveState() const
{
  SaveStateWriter writer;
  writer.writeBytes(SAVE_STATE_MAGIC);
  writer.writeU32(SAVE_STATE_VERSION);
  serializeComponents(writer);
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

  SaveStateWriter backupWriter;
  serializeComponents(backupWriter);

  try {
    deserializeComponents(reader);
  } catch (const std::out_of_range&) {
    SaveStateReader backupReader{ backupWriter.bytes() };
    deserializeComponents(backupReader);
    return std::unexpected("corrupt or truncated save state");
  }
  return {};
}

};
