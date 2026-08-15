export module gbemu:ppu;

import :mmu;
import :hardware_mode;
import :serialization;
import :hard_assert;

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

  // Returns the last *fully rendered* frame, not whatever's currently being
  // drawn into mid-scanline - GameBoy::runNextFrame() can stop anywhere
  // within a frame's timing, not necessarily inside VBlank. Real rendering
  // always writes into m_frameBuffer; runNextTCycle() copies it into
  // m_completedFrameBuffer the instant a frame finishes (LY reaches 144).
  [[nodiscard]] const FrameBuffer& frameBuffer() const;

  // Rendering rules genuinely differ between CgbCompatibility and
  // CgbNative: CgbCompatibility means a DMG-only cartridge running on CGB
  // hardware - real hardware's own DMG-compatibility scheme applies
  // (background/object shade computed exactly as on real DMG hardware via
  // BGP/OBP0/OBP1, then looked up in CGB background palette 0 / object
  // palette 0-1 instead of the fixed grayscale table). CgbNative (a
  // genuinely CGB-aware cartridge) needs VRAM-bank-1 tile attributes,
  // palettes 0-7, and CGB priority rules instead.
  void setHardwareMode(HardwareMode mode) { m_hardwareMode = mode; }

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  HardwareMode m_hardwareMode{ HardwareMode::Dmg };
  enum class Mode : std::uint8_t
  {
    HBlank = 0,
    VBlank = 1,
    OAMSearch = 2,
    PixelTransfer = 3,
  };

  template<typename T>
  class Fifo
  {
  public:
    void push(T pixel)
    {
      hardAssert(m_size < m_buffer.size(), "Fifo::push: already at capacity");
      m_buffer.at((m_head + m_size) % m_buffer.size()) = std::move(pixel);
      ++m_size;
    }

    T pop()
    {
      hardAssert(m_size != 0, "Fifo::pop: empty");
      auto pixel = std::move(m_buffer.at(m_head));
      m_head = (m_head + 1) % m_buffer.size();
      --m_size;
      return pixel;
    }

    // Merges 8 freshly-fetched pixels into the FIFO at [offset, offset+8) -
    // shouldReplace(incoming, existing) decides each one.
    template<typename Predicate>
    void merge(std::size_t offset,
               const std::array<T, 8>& pixels,
               Predicate shouldReplace)
    {
      for (std::size_t i = 0; i < 8; ++i) {
        auto& existing = at(offset + i);
        const auto& incoming = pixels.at(i);
        if (shouldReplace(incoming, existing)) {
          existing = incoming;
        }
      }
    }

    void clear()
    {
      m_head = 0;
      m_size = 0;
    }
    [[nodiscard]] bool empty() const { return m_size == 0; }
    [[nodiscard]] std::size_t size() const { return m_size; }

    void serialize(SaveStateWriter& writer) const
    {
      writer.writeSize(m_head);
      writer.writeSize(m_size);
      for (const auto& pixel : m_buffer) {
        pixel.serialize(writer);
      }
    }

    void deserialize(SaveStateReader& reader)
    {
      m_head = reader.readSize();
      m_size = reader.readSize();
      for (auto& pixel : m_buffer) {
        pixel.deserialize(reader);
      }
    }

  private:
    [[nodiscard]] T& at(std::size_t index)
    {
      hardAssert(index < m_size, "Fifo::at: index >= logical size");
      return m_buffer.at((m_head + index) % m_buffer.size());
    }

    std::array<T, 16> m_buffer{};
    std::size_t m_head{ 0 };
    std::size_t m_size{ 0 };
  };

  struct Object
  {
    std::uint8_t oamIndex{};
    std::uint8_t yPos{};
    std::uint8_t xPos{};
    std::uint8_t tileIndex{};
    std::uint8_t attributes{};
    bool isFetched{ false };

    void serialize(SaveStateWriter& writer) const
    {
      writer.writeU8(oamIndex);
      writer.writeU8(yPos);
      writer.writeU8(xPos);
      writer.writeU8(tileIndex);
      writer.writeU8(attributes);
      writer.writeBool(isFetched);
    }

    void deserialize(SaveStateReader& reader)
    {
      oamIndex = reader.readU8();
      yPos = reader.readU8();
      xPos = reader.readU8();
      tileIndex = reader.readU8();
      attributes = reader.readU8();
      isFetched = reader.readBool();
    }
  };
  class Fetcher
  {
  public:
    enum class Mode : std::uint8_t
    {
      Background,
      Window,
      Object,
    };
    Fetcher(Mmu& mmu, Ppu& ppu)
      : m_mmu(mmu)
      , m_ppu(ppu)
    {
    }
    void runNextTCycle();
    void reset(Mode mode);
    [[nodiscard]] bool isFetchingObject() const
    {
      return m_mode == Mode::Object;
    }

    void serialize(SaveStateWriter& writer) const;
    void deserialize(SaveStateReader& reader);

  private:
    enum class State : std::uint8_t
    {
      ReadTile,
      ReadTileDataLow,
      ReadTileDataHigh,
      Sleep,
      PushToFifo,
    };

    void checkForObject();
    void checkForWindow();

    std::reference_wrapper<Mmu> m_mmu;
    std::reference_wrapper<Ppu> m_ppu;
    State m_mState{ State::ReadTile };
    Mode m_mode{ Mode::Background };
    bool m_rowPushed{ false };
    std::uint8_t m_tileX{ 0 };
    std::uint8_t m_Y{ 0 };
    std::uint8_t m_mTileIndex{ 0 };
    std::uint8_t m_tileAttributes{ 0 };
    std::uint16_t m_lastDotStateChange{ 0 };
    std::uint8_t m_tileDataLow{};
    std::uint8_t m_tileDataHigh{};
    Object m_currentObject{};
  };

  struct BackgroundPixel
  {
    std::uint8_t colorIndex{};
    std::uint8_t palette{};
    bool priority{};

    void serialize(SaveStateWriter& writer) const
    {
      writer.writeU8(colorIndex);
      writer.writeU8(palette);
      writer.writeBool(priority);
    }

    void deserialize(SaveStateReader& reader)
    {
      colorIndex = reader.readU8();
      palette = reader.readU8();
      priority = reader.readBool();
    }
  };

  // objectX/oamIndex exist for drawing-priority resolution when two
  // opaque object pixels overlap: DMG - smaller objectX wins, tie-broken
  // by oamIndex; CGB - oamIndex alone.
  struct ObjectPixel
  {
    std::uint8_t colorIndex{};
    std::uint8_t palette{};
    std::uint8_t objectX{};
    std::uint8_t oamIndex{};
    bool behindBackground{};

    void serialize(SaveStateWriter& writer) const
    {
      writer.writeU8(colorIndex);
      writer.writeU8(palette);
      writer.writeU8(objectX);
      writer.writeU8(oamIndex);
      writer.writeBool(behindBackground);
    }

    void deserialize(SaveStateReader& reader)
    {
      colorIndex = reader.readU8();
      palette = reader.readU8();
      objectX = reader.readU8();
      oamIndex = reader.readU8();
      behindBackground = reader.readBool();
    }
  };

  std::array<Object, 10> m_objects{};
  std::size_t m_objectCount{ 0 };

  std::reference_wrapper<Mmu> m_mmu;
  std::uint8_t m_scanline{ 0 };
  std::uint8_t m_activeWindowRow{ 0 };
  std::uint16_t m_dot{ 0 };
  Mode m_mode{ Mode::OAMSearch };
  // Real hardware: LCDC.7 off forces LY=0/STAT mode=HBlank for as long as
  // it stays off, and re-enabling always restarts a fresh frame at
  // scanline 0/mode 2 - never resumes wherever the PPU was paused.
  bool m_lcdEnabled{ false };
  std::uint8_t m_pixelsRendered{ 0 };
  std::uint8_t m_scx3LowBits{ 0 };
  std::uint8_t m_scxDiscardedCount{ 0 };
  // WX < 7 has no valid on-screen X = WX-7 (it'd be negative) - the window
  // still triggers at screen X=0 in that case, same as WX=7, just with its
  // own leftmost (7-WX) pixels clipped rather than shown.
  std::uint8_t m_windowPixelsToDiscard{ 0 };
  bool m_YCondition{ false };
  Fifo<BackgroundPixel> m_bgWndFifo{};
  Fifo<ObjectPixel> m_objFifo{};
  FrameBuffer m_frameBuffer{};
  FrameBuffer m_completedFrameBuffer{};
  Fetcher m_fetcher{ m_mmu, *this };
  // Snapshot of m_fetcher's state, captured/restored around an object fetch
  // pausing/resuming the background/window fetch it interrupted.
  Fetcher m_savedFetcherState{ m_mmu, *this };

  void incrementDot();
  void handleOAMSearch();
  [[nodiscard]] bool handlePixelTransfer();
  void saveFetcherState();
  void restoreFetcherState();
};

};
