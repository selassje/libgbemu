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
    Mode m_mode{ Mode::OAMSearch};

    void incrementDot();
};

};