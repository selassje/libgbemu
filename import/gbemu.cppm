export module gbemu;

import std;

export import :cpu;
export import :mmu;
export import :ppu;
export import :boot_rom;
export import :regs;

export namespace gbemu {

struct Rgb
{
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};


struct EmulationFrame
{
  std::mdspan<std::uint8_t,
              std::extents<std::size_t, SCREEN_HEIGHT, SCREEN_WIDTH>>
    pixels;
};

class GameBoy
{
public:
  GameBoy()
    : m_ppu(m_mmu)
    , m_cpu(m_mmu, m_ppu)
  {
  }

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  std::expected<EmulationFrame, std::string> runNextFrame();

private:
  Mmu m_mmu;
  // Declared before m_cpu so it's fully constructed before Cpu's
  // constructor receives a reference to it.
  Ppu m_ppu;
  Cpu m_cpu;
};

}
