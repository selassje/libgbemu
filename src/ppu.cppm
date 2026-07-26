module;
#include <functional>
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

  class Fifo
  {
  public:
    void push(std::uint8_t pixel);
    std::uint8_t pop();
    [[nodiscard]] bool empty() const { return m_size == 0; }
    [[nodiscard]] std::size_t size() const { return m_size; }

  private:
    std::array<std::uint8_t, 16> m_buffer{};
    std::size_t m_head{ 0 };
    std::size_t m_size{ 0 };
  };

  class Fetcher
  {
  public:
    Fetcher(Mmu& mmu, Ppu& ppu)
      : m_mmu(mmu)
      , m_ppu(ppu)
    {
    }

    void runNextTCycle();

  private:
    enum class State : std::uint8_t
    {
      ReadTile,
      ReadTileDataLow,
      ReadTileDataHigh,
      Sleep,
      PushToFifo,
    };

    std::reference_wrapper<Mmu> m_mmu;
    std::reference_wrapper<Ppu> m_ppu;
    State m_mState{ State::ReadTile };
    std::uint8_t m_X{ 0 };
    std::uint8_t m_mTileIndex{ 0 };
    std::uint16_t m_lastDotStateChange{ 0 };
    std::uint8_t m_tileDataLow{};
    std::uint8_t m_tileDataHigh{};
  };

  std::reference_wrapper<Mmu> m_mmu;
  std::uint8_t m_scanline{ 0 };
  std::uint16_t m_dot{ 0 };
  Mode m_mode{ Mode::OAMSearch };
  std::uint8_t m_nextPixelXToRender{ 0 };
  std::uint8_t m_scx3LowBits{ 0 };
  std::uint8_t m_scxDiscardedCount{ 0 };
  Fifo m_bgWndFifo{};
  Fifo m_objFifo{};
  // Placeholder for rendered pixels: raw 2bpp color indices (160x144), not
  // yet palette-mapped to actual shades/colors.
  std::array<std::uint8_t, std::size_t{ 160 } * 144> m_frameBuffer{}; // NOLINT(readability-magic-numbers)
  Fetcher m_fetcher{ m_mmu, *this };

  void incrementDot();
  void handleHBlank();
  void handleVBlank();
  void handleOAMSearch();
  [[nodiscard]] bool handlePixelTransfer();
};

};