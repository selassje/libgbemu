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
              std::extents<std::size_t, SCREEN_HEIGHT, SCREEN_WIDTH, 3>>
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

  // Power-cycle equivalent: re-runs the exact same boot sequence against
  // the already-loaded cartridge, without needing it re-supplied - the
  // same cartridge stays "inserted", same as a real Game Boy's power
  // switch off/on.
  [[nodiscard]] std::expected<void, std::string> reset();

  std::expected<EmulationFrame, std::string> runNextFrame();
  void setButtonState(Button button, bool pressed);

private:
  // Reconstructs Mmu/Ppu/Cpu from scratch (guaranteeing every field
  // returns to its true declared default, rather than a hand-maintained
  // per-field reset that could silently miss one) and re-runs the same
  // load sequence loadRom()/reset() both need, operating on whatever's
  // currently in m_romBytes.
  [[nodiscard]] std::expected<void, std::string> initializeFromRom();

  Mmu m_mmu;
  // Declared before m_cpu so it's fully constructed before Cpu's
  // constructor receives a reference to it.
  Ppu m_ppu;
  Cpu m_cpu;
  // Whether the cartridge itself declares CGB awareness (header byte
  // 0x0143, bit 7) - kept for gating actual CGB-exclusive hardware
  // features (VRAM banking, palette RAM, double speed, ...) once those
  // exist. Deliberately not used to decide which boot ROM runs: real
  // hardware always boots as whichever console it physically is, and lets
  // the boot ROM itself branch on this same flag to decide compatibility
  // vs. native mode - see GameBoy::initializeFromRom().
  bool m_isCgb{ false };
  // Kept so reset() can re-run initializeFromRom() without the caller
  // needing to re-supply the same ROM bytes.
  std::vector<std::uint8_t> m_romBytes;
};

}
