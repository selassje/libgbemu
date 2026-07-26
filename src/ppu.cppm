export module gbemu:ppu;

import :mmu;

namespace gbemu {

class Ppu // NOLINT(misc-use-internal-linkage)
{
public:
  explicit Ppu(Mmu& mmu)
    : m_mmu(mmu)
  {
  }
  void runNextTCycle();

  [[nodiscard]] std::uint16_t dot() const { return m_dot; }

private:
  enum class Mode : std::uint8_t
  {
    HBlank = 0,
    VBlank = 1,
    OAMSearch = 2,
    PixelTransfer = 3,
  };

  std::reference_wrapper<Mmu> m_mmu;
  std::uint8_t m_scanline{ 0 };
  std::uint16_t m_dot{ 0 };
  Mode m_mode{ Mode::OAMSearch };
  std::uint8_t m_nextPixelXToRender{ 0 };

  void incrementDot();

  void handleHBlank();
  void handleVBlank();
  void handleOAMSearch();
  void handlePixelTransfer();
};

};