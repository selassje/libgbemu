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

  // Generic ring buffer shared by the background/window and object FIFOs.
  // Carries no pixel-specific policy (transparency, priority, ...) of its
  // own - merge()'s shouldReplace callback supplies that, so this stays
  // reusable for both a plain std::uint8_t color index and the richer
  // ObjectPixel.
  template<typename T>
  class Fifo
  {
  public:
    void push(T pixel)
    {
      if (m_size >= m_buffer.size()) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass) - MSVC STL's
        // std::out_of_range base-class chain isn't visible to clang-tidy
        // through `import std;`'s module boundary; not reproducible on
        // libc++ (dev_ninja_clang_tidy_linux builds this file clean).
        throw std::out_of_range("Fifo::push: already at capacity");
      }
      m_buffer.at((m_head + m_size) % m_buffer.size()) = std::move(pixel);
      ++m_size;
    }

    T pop()
    {
      if (m_size == 0) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass) - see push()'s comment.
        throw std::out_of_range("Fifo::pop: empty");
      }
      auto pixel = std::move(m_buffer.at(m_head));
      m_head = (m_head + 1) % m_buffer.size();
      --m_size;
      return pixel;
    }

    // Merges 8 freshly-fetched pixels into the FIFO at [offset, offset+8),
    // one at a time, positionally - pixels[i] is only ever compared
    // against and possibly replaces whatever is already at slot offset+i.
    // shouldReplace(incoming, existing) decides each one; callers are
    // responsible for having already grown the FIFO to cover that range
    // (e.g. via push()) before calling this.
    template<typename Predicate>
    void merge(std::size_t offset,
               const std::array<T, 8>& pixels,
               Predicate shouldReplace)
    {
      for (std::size_t i = 0; i < 8; ++i) { // NOLINT(readability-magic-numbers)
        auto& existing = at(offset + i);
        const auto& incoming = pixels.at(i);
        if (shouldReplace(incoming, existing)) {
          existing = incoming;
        }
      }
    }

    void clear() { m_head = m_size = 0; }
    [[nodiscard]] bool empty() const { return m_size == 0; }
    [[nodiscard]] std::size_t size() const { return m_size; }

  private:
    [[nodiscard]] T& at(std::size_t index)
    {
      if (index >= m_size) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass) - see push()'s comment.
        throw std::out_of_range("Fifo::at: index >= logical size");
      }
      return m_buffer.at((m_head + index) % m_buffer.size());
    }

    std::array<T, 16> m_buffer{}; // NOLINT(readability-magic-numbers)
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

  struct Object
  {
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
  struct ObjectPixel
  {
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
  Fifo<std::uint8_t> m_bgWndFifo{};
  Fifo<ObjectPixel> m_objFifo{};
  FrameBuffer m_frameBuffer{};
  Fetcher m_fetcher{ m_mmu, *this };

  void incrementDot();
  void handleHBlank();
  void handleVBlank();
  void handleOAMSearch();
  [[nodiscard]] bool handlePixelTransfer();
};

};