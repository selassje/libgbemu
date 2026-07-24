module gbemu;

namespace gbemu {

[[nodiscard]] std::expected<void, std::string>
GameBoy::loadRom(std::span<const std::uint8_t> rom)

{
  return m_mmu.loadRom(rom);
}

};

std::expected<void, std::string>
gbemu::GameBoy::runNextInstruction()
{
  return m_cpu.runNextInstruction();
}