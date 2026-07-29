export module gbemu:ppu;

import :mmu;

namespace gbemu {

export std::size_t constexpr SCREEN_WIDTH = 160;
export std::size_t constexpr SCREEN_HEIGHT = 144;
class Ppu // NOLINT(misc-use-internal-linkage)
{
public:
  explicit Ppu(Mmu& mmu)
    : m_mmu(mmu)
  {
  }
  void runNextTCycle();

  [[nodiscard]] std::uint16_t dot() const { return m_dot; }
  using FrameBuffer =
    std::array<std::uint8_t, gbemu::SCREEN_WIDTH * gbemu::SCREEN_HEIGHT * 3>;

  FrameBuffer& frameBuffer();

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
    void clear() { m_head = m_size = 0; }
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
    enum class Mode : std::uint8_t
    {
      Background,
      Window,

    };
    Fetcher(Mmu& mmu, Ppu& ppu)
      : m_mmu(mmu)
      , m_ppu(ppu)
    {
    }
    void runNextTCycle();
    void reset(Mode mode);

  private:
    enum class State : std::uint8_t
    {
      ReadTile,
      ReadTileDataLow,
      ReadTileDataHigh,
      Sleep,
      PushToFifo,
    };

    void checkForWindow();

    std::reference_wrapper<Mmu> m_mmu;
    std::reference_wrapper<Ppu> m_ppu;
    State m_mState{ State::ReadTile };
    Mode m_mode{ Mode::Background };
    bool m_rowPushed{ false };
    std::uint8_t m_tileX{ 0 };
    std::uint8_t m_Y{ 0 };
    std::uint8_t m_mTileIndex{ 0 };
    std::uint16_t m_lastDotStateChange{ 0 };
    std::uint8_t m_tileDataLow{};
    std::uint8_t m_tileDataHigh{};
  };

  struct Object {
    std::uint16_t oamAdress{};
    std::uint8_t yPos{};
    std::uint8_t xPos{};
    std::uint8_t tileIndex{};
    std::uint8_t attributes{};
  };

  // A decoded object-FIFO pixel. objectX and oamIndex exist purely for
  // drawing-priority resolution when two opaque object pixels overlap
  // (DMG: smaller objectX wins, tie-broken by oamIndex; CGB: oamIndex
  // alone) - deliberately not relying on sprite fetch order matching
  // priority order, which would be correct today but fragile against
  // future changes to fetch scheduling. objectX stores the raw OAM byte
  // (X+8) unmodified - priority is a relative comparison, so the +8
  // offset never changes which pixel wins.
  struct ObjectPixel {
    std::uint8_t colorIndex{};
    std::uint8_t palette{};
    std::uint8_t objectX{};
    std::uint8_t oamIndex{};
    bool behindBackground{};
  };

  std::array<Object, 10> m_objects{}; // NOLINT(readability-magic-numbers)
  std::size_t m_objectCount{ 0 };

  std::reference_wrapper<Mmu> m_mmu;
  std::uint8_t m_scanline{ 0 };
  std::uint8_t m_activeWindowRow{ 0 };
  std::uint16_t m_dot{ 0 };
  Mode m_mode{ Mode::OAMSearch };
  std::uint8_t m_pixelsRendered{ 0 };
  std::uint8_t m_scx3LowBits{ 0 };
  std::uint8_t m_scxDiscardedCount{ 0 };
  bool m_YCondition{ false };
  Fifo m_bgWndFifo{};
  Fifo m_objFifo{};
  FrameBuffer m_frameBuffer{};
  Fetcher m_fetcher{ m_mmu, *this };

  void incrementDot();
  void handleHBlank();
  void handleVBlank();
  void handleOAMSearch();
  [[nodiscard]] bool handlePixelTransfer();
};

};