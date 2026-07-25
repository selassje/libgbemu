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
    : m_cpu(m_mmu), m_ppu(m_mmu)
  {
  }

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  std::expected<void, std::string> runNextFrame();

private:
  Mmu m_mmu;
  Cpu m_cpu;
  Ppu m_ppu;
};

}
