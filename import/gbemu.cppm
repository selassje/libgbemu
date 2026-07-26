export module gbemu;

import std;

export import :cpu;
export import :mmu;
export import :ppu;
export import :boot_rom;

export namespace gbemu {

class GameBoy
{
public:
  GameBoy()
    : m_ppu(m_mmu), m_cpu(m_mmu, m_ppu)
  {
  }

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  std::expected<void, std::string> runNextFrame();

private:
  Mmu m_mmu;
  // Declared before m_cpu so it's fully constructed before Cpu's
  // constructor receives a reference to it.
  Ppu m_ppu;
  Cpu m_cpu;
};

}
