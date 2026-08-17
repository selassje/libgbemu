module gbemu;

import :regs;

namespace {

constexpr std::uint8_t TOTAL_SCANLINES = 154;
constexpr std::uint8_t LAST_VISIBLE_SCANLINE = 143;
constexpr std::uint16_t NORMAL_MODE_3_DOT = 84;
constexpr std::uint16_t NORMAL_RENDERER_START_DOT = 89;
constexpr std::uint16_t STARTUP_MODE_3_DOT = 87;
constexpr std::uint16_t STARTUP_RENDERER_START_DOT = 92;
constexpr std::uint16_t DOTS_PER_SCANLINE = 456;

constexpr std::uint8_t VBLANK_INTERRUPT_BIT = 0x01;
constexpr std::uint8_t FIRST_VBLANK_SCANLINE = LAST_VISIBLE_SCANLINE + 1;
constexpr std::uint8_t LCD_ENABLE_BIT = 0x80;

// Standard grayscale mapping for the DMG's 4 shades, indexed by BGP-mapped
// shade (0 = lightest, 3 = darkest).
constexpr std::array<std::array<std::uint8_t, 3>, 4> DMG_PALETTE = { {
  { 0xFF, 0xFF, 0xFF },
  { 0xAA, 0xAA, 0xAA },
  { 0x55, 0x55, 0x55 },
  { 0x00, 0x00, 0x00 },
} };

// Expands a CGB palette color (15-bit RGB555, packed 0bBBBBBGGGGGRRRRR) to
// 8-bit RGB. Bit-replicates the top 3 bits into the low 3 so a component's
// full 5-bit range 0-31 maps onto the full 8-bit range 0-255 (0->0,
// 31->255) instead of leaving it capped at 248 - not a hardware-accurate
// color-correction curve (real CGB hardware's LCD has its own non-linear
// response that this doesn't attempt to model).
std::array<std::uint8_t, 3>
cgbColorToRgb(std::uint16_t color)
{
  constexpr unsigned componentMask = 0x1FU;
  constexpr unsigned redShift = 0U;
  constexpr unsigned greenShift = 5U;
  constexpr unsigned blueShift = 10U;
  constexpr unsigned expandLowShift = 3U;
  constexpr unsigned expandHighShift = 2U;
  const auto expand = [](unsigned component5) -> std::uint8_t {
    return static_cast<std::uint8_t>((component5 << expandLowShift) |
                                     (component5 >> expandHighShift));
  };
  const auto unsignedColor = static_cast<unsigned>(color);
  return { expand((unsignedColor >> redShift) & componentMask),
           expand((unsignedColor >> greenShift) & componentMask),
           expand((unsignedColor >> blueShift) & componentMask) };
}

}

namespace gbemu {

void
Ppu::Fetcher::checkForObject()
{
  if (m_mode == Mode::Object) {
    return;
  }
  // Only the first m_objectCount entries are valid for this scanline -
  // slots beyond it hold stale data from whichever earlier scanline last
  // wrote there.
  auto objects =
    std::span{ m_ppu.get().m_objects }.first(m_ppu.get().m_objectCount);
  auto it = std::ranges::find_if(objects, [&](const auto& object) {
    const auto fetchX = static_cast<int>(m_ppu.get().m_pixelsRendered) -
                        m_ppu.get().m_initialPipelinePixelsToDiscard;
    return !object.isFetched &&
           (fetchX + 8 >= object.xPos);
  });

  if (it == objects.end()) {
    return;
  }

  // Mesen starts the object fetch only after the background fetcher has
  // reached step 5 and the background FIFO contains pixels.  Sleep is the
  // state immediately after our high-byte/step-5 operation; PushToFifo
  // represents the following push opportunities.
  const bool backgroundReachedStep5 =
    m_mState == State::Sleep || m_mState == State::PushToFifo;
  if (!backgroundReachedStep5 || m_ppu.get().m_bgWndFifo.empty()) {
    return;
  }

  it->isFetched = true;
  m_currentObject = *it;
  m_ppu.get().saveFetcherState();
  reset(Mode::Object);
}

void
Ppu::Fetcher::checkForWindow()
{
  if (m_mode == Mode::Window || m_mode == Mode::Object) {
    return;
  }
  const auto wx = m_mmu.get().readByte(regs::WX);
  const auto windowEnabled = (m_mmu.get().readByte(regs::LCDC) & 0x20U) != 0;
  const auto yCondition = m_ppu.get().m_YCondition;
  // wx + 7 == m_pixelsRendered would never match any wx below 7, since
  // m_pixelsRendered can't go negative - real hardware still triggers at
  // screen X=0 for wx < 7 (see m_windowPixelsToDiscard's own comment for
  // the clipping that handles the rest), so this compares the other way
  // around instead, clamped at 0.
  constexpr unsigned wxOffset = 7;
  const auto windowStartX = wx >= wxOffset ? (wx - wxOffset) : 0U;
  const auto fetchX = static_cast<int>(m_ppu.get().m_pixelsRendered) -
                      m_ppu.get().m_initialPipelinePixelsToDiscard;
  const auto wxReached =
    fetchX >= 0 && static_cast<unsigned>(fetchX) == windowStartX;
  if (windowEnabled && yCondition && wxReached) {
    reset(Mode::Window);
  }
}

void
Ppu::Fetcher::runNextTCycle()
{
  // Window must be checked first: its trigger is an exact-equality check
  // on m_pixelsRendered, unlike the object trigger (deliberately >= so it
  // survives being stalled) - checking window first lets it switch mode
  // before an object fetch can steal the dot.
  checkForWindow();
  checkForObject();

  const auto elapsedDots = m_ppu.get().m_dot - m_lastDotStateChange;
  const auto currentState = m_mState;
  constexpr std::uint16_t tileDataBlock0 = 0x8000;
  constexpr std::uint16_t tileDataBlock1 = 0x8800;
  constexpr std::uint16_t tileDataBlock2 = 0x9000;

  const auto pushTileRowToFifo = [this]() {
    if (m_rowPushed || !m_ppu.get().m_bgWndFifo.empty()) {
      return false;
    }
    const bool native = m_ppu.get().m_hardwareMode == HardwareMode::CgbNative;
    constexpr unsigned paletteMask = 0x07U;
    constexpr unsigned xFlipBit = 0x20U;
    constexpr unsigned priorityBit = 0x80U;
    const bool xFlip =
      native && (static_cast<unsigned>(m_tileAttributes) & xFlipBit) != 0;
    const auto palette = static_cast<std::uint8_t>(
      native ? static_cast<unsigned>(m_tileAttributes) & paletteMask : 0);
    const bool priority =
      native && (static_cast<unsigned>(m_tileAttributes) & priorityBit) != 0;
    for (unsigned i = 0; i < 8; ++i) {
      const unsigned bit = xFlip ? i : (7 - i);
      const auto colorIndex = static_cast<std::uint8_t>(
        (((static_cast<unsigned>(m_tileDataHigh) >> bit) & 1U) << 1U) |
        ((static_cast<unsigned>(m_tileDataLow) >> bit) & 1U));
      m_ppu.get().m_bgWndFifo.push({ colorIndex, palette, priority });
    }
    m_rowPushed = true;
    return true;
  };

  // Pads the object FIFO up to 8 pixels (transparent, lowest priority) if it
  // was short, then merges the just-fetched object row into it, one pixel
  // at a time: an opaque incoming pixel always replaces an existing
  // transparent one; between two opaque pixels, DMG priority (smaller
  // objectX wins, tied objectX broken by smaller oamIndex) decides.
  const auto mergeObjectRowIntoFifo = [this]() {
    constexpr std::uint8_t xFlipMask = 0x20;
    constexpr std::uint8_t dmgPaletteMask = 0x10;
    constexpr std::uint8_t cgbPaletteMask = 0x07;
    constexpr std::uint8_t priorityMask = 0x80;

    const bool xFlip = (m_currentObject.attributes & xFlipMask) != 0;
    const bool native = m_ppu.get().m_hardwareMode == HardwareMode::CgbNative;
    std::uint8_t palette = 0;
    if (native) {
      palette =
        static_cast<std::uint8_t>(m_currentObject.attributes & cgbPaletteMask);
    } else if ((m_currentObject.attributes & dmgPaletteMask) != 0) {
      palette = 1;
    }
    const bool behindBackground =
      (m_currentObject.attributes & priorityMask) != 0;

    std::array<ObjectPixel, 8> pixels{};
    for (unsigned i = 0; i < 8; ++i) {
      const unsigned bit = xFlip ? i : (7 - i);
      const auto colorIndex = static_cast<std::uint8_t>(
        (((static_cast<unsigned>(m_tileDataHigh) >> bit) & 1U) << 1U) |
        ((static_cast<unsigned>(m_tileDataLow) >> bit) & 1U));
      pixels.at(i) = { colorIndex,
                       palette,
                       m_currentObject.xPos,
                       m_currentObject.oamIndex,
                       behindBackground };
    }

    auto& objFifo = m_ppu.get().m_objFifo;
    while (objFifo.size() < 8) {
      objFifo.push({});
    }
    const bool cgbOamPriority =
      native && (m_mmu.get().readByte(regs::OPRI) & 0x01U) == 0;
    objFifo.merge(
      0, pixels, [cgbOamPriority](const auto& incoming, const auto& existing) {
        if (incoming.colorIndex == 0) {
          return false;
        }
        if (existing.colorIndex == 0) {
          return true;
        }
        if (cgbOamPriority) {
          return incoming.oamIndex < existing.oamIndex;
        }
        return std::tie(incoming.objectX, incoming.oamIndex) <
               std::tie(existing.objectX, existing.oamIndex);
      });
  };

  switch (m_mState) {
    case State::ReadTile:
      if (elapsedDots >= 2) {

        const auto lcdc = m_mmu.get().readByte(regs::LCDC);
        const auto lcdcBit3 = (lcdc & 0x08U) != 0;
        const auto lcdcBit6 = (lcdc & 0x40U) != 0;
        const auto isWindow = (m_mode == Mode::Window);
        const std::uint16_t tileMapBaseAddress = [&]() -> std::uint16_t {
          if ((isWindow && lcdcBit6) || (!isWindow && lcdcBit3)) {
            return 0x9C00;
          }
          return 0x9800;
        }();

        const auto& [tileX,
                     tileY] = [&]() -> std::pair<std::uint8_t, std::uint8_t> {
          if (m_mode == Mode::Window) {
            return { static_cast<std::uint8_t>(m_tileX % 32),
                     static_cast<std::uint8_t>(m_Y % 256) };
          }
          const std::uint8_t tileXPrim =
            (m_tileX + (m_mmu.get().readByte(regs::SCX) / 8)) % 32;
          const std::uint8_t tileYPrim =
            (m_Y + m_mmu.get().readByte(regs::SCY)) % 256;
          return { tileXPrim, tileYPrim };
        }();

        const auto tileMapAddress = static_cast<std::uint16_t>(
          tileMapBaseAddress + ((tileY / 8) * 32) + tileX);
        constexpr std::uint8_t vramBank0 = 0;
        m_mTileIndex = m_mmu.get().readVram(vramBank0, tileMapAddress);
        constexpr std::uint8_t vramBank1 = 1;
        m_tileAttributes = m_ppu.get().m_hardwareMode == HardwareMode::CgbNative
                             ? m_mmu.get().readVram(vramBank1, tileMapAddress)
                             : 0;
        m_mState = State::ReadTileDataLow;
      }
      break;
    case State::ReadTileDataLow:
    case State::ReadTileDataHigh:
      // Mesen's object fetcher clocks steps 0..5: tile/index at step 1,
      // low data at step 3, and high data plus push at step 5.  Object reset
      // starts at step 0 here, so the low-byte state lasts 3 dots and the
      // following high-byte state lasts 2.  Background/window bytes retain
      // their normal two-dot cadence.
      if (elapsedDots >=
          (m_mode == Mode::Object && m_mState == State::ReadTileDataLow ? 3
                                                                        : 2)) {
        const bool isHighByte = (m_mState == State::ReadTileDataHigh);
        std::uint8_t tileByte{};
        if (m_mode == Mode::Object) {
          // Objects always use unsigned tile indexing out of tile block 0
          // ($8000-8FFF) - unlike background/window, there's no LCDC.4
          // dependent signed/$9000-relative addressing for objects.
          constexpr std::uint8_t objectSizeMask = 0x04;
          constexpr std::uint8_t yFlipMask = 0x40;
          constexpr std::uint8_t topTileMask = 0xFE;
          constexpr std::uint8_t bottomTileBit = 0x01;
          const auto lcdc = m_mmu.get().readByte(regs::LCDC);
          const bool is8x16 = (lcdc & objectSizeMask) != 0;
          const unsigned objectHeight = is8x16 ? 16 : 8;
          const bool yFlip = (m_currentObject.attributes & yFlipMask) != 0;
          const auto scanlinePlus16 =
            static_cast<unsigned>(m_ppu.get().m_scanline) + 16;
          const unsigned objectRow = scanlinePlus16 - m_currentObject.yPos;
          const unsigned effectiveRow =
            yFlip ? (objectHeight - 1 - objectRow) : objectRow;
          auto tileIndex = m_currentObject.tileIndex;
          unsigned rowWithinTile = effectiveRow;
          if (is8x16) {
            if (effectiveRow < 8) {
              tileIndex = static_cast<std::uint8_t>(tileIndex & topTileMask);
            } else {
              tileIndex = static_cast<std::uint8_t>(tileIndex | bottomTileBit);
              rowWithinTile = effectiveRow - 8;
            }
          }
          const auto rowOffset = static_cast<std::uint16_t>(
            (rowWithinTile * 2) + (isHighByte ? 1 : 0));
          const auto tileOffset =
            static_cast<std::uint16_t>((tileIndex * 16) + rowOffset);
          constexpr unsigned vramBankBit = 0x08U;
          const auto vramBank = static_cast<std::uint8_t>(
            m_ppu.get().m_hardwareMode == HardwareMode::CgbNative &&
                (static_cast<unsigned>(m_currentObject.attributes) &
                 vramBankBit) != 0
              ? 1
              : 0);
          tileByte =
            m_mmu.get().readVram(vramBank, tileDataBlock0 + tileOffset);
        } else {
          const auto scy =
            m_mode == Mode::Window ? 0 : m_mmu.get().readByte(regs::SCY);
          auto row = static_cast<unsigned>((m_Y + scy) % 8);
          const bool native =
            m_ppu.get().m_hardwareMode == HardwareMode::CgbNative;
          constexpr unsigned yFlipBit = 0x40U;
          constexpr unsigned vramBankBit = 0x08U;
          if (native &&
              (static_cast<unsigned>(m_tileAttributes) & yFlipBit) != 0) {
            row = 7 - row;
          }
          const auto rowOffset =
            static_cast<std::uint16_t>((row * 2) + (isHighByte ? 1 : 0));
          const auto vramBank = static_cast<std::uint8_t>(
            native &&
                (static_cast<unsigned>(m_tileAttributes) & vramBankBit) != 0
              ? 1
              : 0);
          if (m_mTileIndex >= 128) {
            const auto tileOffset = static_cast<std::uint16_t>(
              ((m_mTileIndex - 128) * 16) + rowOffset);
            tileByte =
              m_mmu.get().readVram(vramBank, tileDataBlock1 + tileOffset);
          } else {
            const auto tileOffset =
              static_cast<std::uint16_t>((m_mTileIndex * 16) + rowOffset);
            const auto lcdc = m_mmu.get().readByte(regs::LCDC);
            if ((lcdc & 0x10U) != 0) {
              tileByte =
                m_mmu.get().readVram(vramBank, tileDataBlock0 + tileOffset);
            } else {
              tileByte =
                m_mmu.get().readVram(vramBank, tileDataBlock2 + tileOffset);
            }
          }
        }
        if (isHighByte) {
          m_tileDataHigh = tileByte;
          if (m_mode == Mode::Object) {
            mergeObjectRowIntoFifo();
            m_ppu.get().restoreFetcherState();
            m_lastDotStateChange = m_ppu.get().m_dot;
          } else {
            m_mState = State::Sleep;
            pushTileRowToFifo();
          }
        } else {
          m_tileDataLow = tileByte;
          m_mState = State::ReadTileDataHigh;
        }
      }
      break;
    case State::Sleep:
      // Sleep performs no fetch work, but the independent push circuit gets
      // its two ordinary opportunities here (steps 6 and 7).  The extra
      // step-5 attempt already happened alongside the high-byte read above.
      pushTileRowToFifo();
      if (elapsedDots >= 2) {
        if (m_rowPushed) {
          m_mState = State::ReadTile;
          m_rowPushed = false;
          ++m_tileX;
        } else {
          m_mState = State::PushToFifo;
        }
      }
      break;
    case State::PushToFifo:
      pushTileRowToFifo();
      if (m_rowPushed) {
        m_mState = State::ReadTile;
        m_rowPushed = false;
        ++m_tileX;
      }
      break;
  }
  if (currentState != m_mState) {
    m_lastDotStateChange = m_ppu.get().m_dot;
  }
};

void
Ppu::Fetcher::restartCurrentFetch()
{
  m_mState = State::ReadTile;
  m_rowPushed = false;
  m_lastDotStateChange = m_ppu.get().m_dot;
}

void
Ppu::Fetcher::reset(Mode mode)
{
  m_mState = State::ReadTile;
  m_rowPushed = false;
  m_tileX = 0;
  m_lastDotStateChange = m_ppu.get().m_dot;
  m_mode = mode;
  if (m_mode == Mode::Window) {
    m_Y = m_ppu.get().m_activeWindowRow;
    m_ppu.get().m_activeWindowRow += 1;
    // The window has no SCX-driven fine scroll of its own, so this just
    // neutralizes the background's own SCX discard check for the rest of
    // the scanline.
    m_ppu.get().m_scxDiscardedCount = m_ppu.get().m_scx3LowBits;
    // WX < 7 triggers here at screen X=0 same as WX == 7, but still owes
    // (7 - WX) pixels of clipping from its own left edge.
    constexpr std::uint8_t wxOffset = 7;
    const auto wx = m_mmu.get().readByte(regs::WX);
    m_ppu.get().m_windowPixelsToDiscard =
      wx < wxOffset ? static_cast<std::uint8_t>(wxOffset - wx) : 0;
  } else {
    m_Y = m_ppu.get().m_scanline;
  }
  if (m_mode == Mode::Object) {
    m_mState = State::ReadTileDataLow;
  } else {
    m_ppu.get().m_bgWndFifo.clear();
  }
}

void
Ppu::Fetcher::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(static_cast<std::uint8_t>(m_mState));
  writer.writeU8(static_cast<std::uint8_t>(m_mode));
  writer.writeBool(m_rowPushed);
  writer.writeU8(m_tileX);
  writer.writeU8(m_Y);
  writer.writeU8(m_mTileIndex);
  writer.writeU8(m_tileAttributes);
  writer.writeU16(m_lastDotStateChange);
  writer.writeU8(m_tileDataLow);
  writer.writeU8(m_tileDataHigh);
  m_currentObject.serialize(writer);
}

void
Ppu::Fetcher::deserialize(SaveStateReader& reader)
{
  m_mState = static_cast<State>(reader.readU8());
  m_mode = static_cast<Mode>(reader.readU8());
  m_rowPushed = reader.readBool();
  m_tileX = reader.readU8();
  m_Y = reader.readU8();
  m_mTileIndex = reader.readU8();
  m_tileAttributes = reader.readU8();
  m_lastDotStateChange = reader.readU16();
  m_tileDataLow = reader.readU8();
  m_tileDataHigh = reader.readU8();
  m_currentObject.deserialize(reader);
}

void
Ppu::runNextTCycle()
{
  const bool lcdEnabled =
    (m_mmu.get().readByte(regs::LCDC) & LCD_ENABLE_BIT) != 0;

  if (!lcdEnabled) {
    if (m_lcdEnabled) {
      // Real hardware: disabling the LCD immediately forces LY=0 and STAT
      // mode=HBlank, not "stay wherever it was" - a game (e.g. Tetris,
      // which disables mid-VBlank at LY=148) can rely on this to reset
      // scanline/mode state before reinitializing VRAM/OAM.
      m_lcdEnabled = false;
      m_lcdStartupLine = false;
      m_dot = 0;
      m_scanline = 0;
      m_mode = Mode::HBlank;
      m_pixelsRendered = 0;
      m_activeWindowRow = 0;
      m_YCondition = false;
      m_bgWndFifo.clear();
      m_objFifo.clear();
      m_completedFrameBuffer.fill(0xFF);
      m_mmu.get().writeByte(regs::LY, m_scanline);
      m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
      m_mmu.get().updateStatCoincidence(m_scanline ==
                                        m_mmu.get().readByte(regs::LYC));
    }
    // The PPU is completely inert while LCDC bit 7 stays clear - no
    // dot/scanline/mode advancement, no interrupts.
    return;
  }

  if (!m_lcdEnabled) {
    // Real hardware: re-enabling the LCD always restarts a fresh frame at
    // scanline 0 - never resumes whatever mode/scanline it was paused at.
    m_lcdEnabled = true;
    m_lcdStartupLine = true;
    m_blankFirstFrame = true;
    m_dot = 7;
    m_scanline = 0;
    // The first scanline after LCD-on performs the mode-2 work internally,
    // but STAT reports mode 0 until its delayed pixel-transfer transition.
    m_mode = Mode::HBlank;
    m_completedFrameBuffer.fill(0xFF);
    m_mmu.get().writeByte(regs::LY, m_scanline);
    m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
    m_mmu.get().updateStatCoincidence(m_scanline ==
                                      m_mmu.get().readByte(regs::LYC));
  }

  switch (m_mode) {
    case Mode::HBlank:
      if (m_lcdStartupLine) {
        handleOAMSearch();
      }
      break;
    case Mode::VBlank:
      // Both are genuinely idle time on real hardware - no fetcher/FIFO
      // activity, nothing to render. Mode transitions, LY updates, and the
      // VBlank interrupt are all already handled by incrementDot().
      break;
    case Mode::OAMSearch:
      handleOAMSearch();
      break;
    case Mode::PixelTransfer:
      if (m_dot < (m_lcdStartupLine ? STARTUP_RENDERER_START_DOT
                                    : NORMAL_RENDERER_START_DOT)) {
        break;
      }
      if (handlePixelTransfer()) {
        m_mode = Mode::HBlank;
        m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
        if (m_scanline <= LAST_VISIBLE_SCANLINE) {
          m_mmu.get().notifyHBlankStart();
        }
      }
      break;
  }

  incrementDot();
}

void
Ppu::incrementDot()
{
  ++m_dot;
  if (m_dot >= DOTS_PER_SCANLINE) {
    m_dot = 0;
    m_pixelsRendered = 0;
    m_scanline = static_cast<std::uint8_t>(m_scanline + 1) % TOTAL_SCANLINES;
    m_mmu.get().writeByte(regs::LY, m_scanline);
    m_mmu.get().updateStatCoincidence(m_scanline ==
                                      m_mmu.get().readByte(regs::LYC));
    if (m_scanline > LAST_VISIBLE_SCANLINE) {
      m_mode = Mode::VBlank;
      m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
      m_activeWindowRow = 0;
      m_YCondition = false;
      if (m_scanline == FIRST_VBLANK_SCANLINE) {
        const auto interruptFlags = m_mmu.get().readByte(regs::IF);
        m_mmu.get().writeByte(
          regs::IF,
          static_cast<std::uint8_t>(interruptFlags | VBLANK_INTERRUPT_BIT));
        if (m_blankFirstFrame) {
          m_blankFirstFrame = false;
        } else {
          m_completedFrameBuffer = m_frameBuffer;
        }
      }
    } else {
      // The PPU starts scanning OAM internally at the line boundary, but
      // STAT's externally-observable timing is staggered.  On LY 1-143 the
      // OAM interrupt condition becomes active at the line-boundary event;
      // the reported mode changes to OAM at dot 4.  LY 0 has no early
      // condition and gets both at dot 4.  This is the CPU-visible timing
      // corresponding to Mesen's cycle-2/cycle-4 split and is required for the
      // Mealybug palette test's LY-0/LY-1 interrupt paths to converge.
      m_mode = Mode::OAMSearch;
      if (m_scanline > 0) {
        // In libgbemu's tick convention the line-boundary callback is the
        // CPU-visible equivalent of Mesen's early cycle-2 IRQ condition.
        m_mmu.get().triggerStatOamInterrupt();
      }
    }
  } else {
    if (m_scanline <= LAST_VISIBLE_SCANLINE) {
      if (!m_lcdStartupLine && m_dot == 4) {
        m_mmu.get().updateStatMode(
          static_cast<std::uint8_t>(Mode::OAMSearch), m_scanline == 0);
      }
      const auto mode3Dot = m_lcdStartupLine ? STARTUP_MODE_3_DOT
                                             : NORMAL_MODE_3_DOT;
      const auto rendererStartDot =
        m_lcdStartupLine ? STARTUP_RENDERER_START_DOT
                         : NORMAL_RENDERER_START_DOT;
      if (m_dot == mode3Dot) {
        m_mode = Mode::PixelTransfer;
        m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
      }
      if (m_dot == rendererStartDot) {
        constexpr std::uint8_t scxLow3BitsMask = 0x07;
        m_scx3LowBits = static_cast<std::uint8_t>(
          m_mmu.get().readByte(regs::SCX) & scxLow3BitsMask);
        m_scxDiscardedCount = 0;
        constexpr std::uint8_t initialPipelinePixels = 8;
        m_initialPipelinePixelsToDiscard = initialPipelinePixels;
        m_windowPixelsToDiscard = 0;
        m_objFifo.clear();
        m_fetcher.reset(Fetcher::Mode::Background);
        for (unsigned i = 0; i < initialPipelinePixels; ++i) {
          m_bgWndFifo.push({});
        }
        if (!m_YCondition && m_scanline == m_mmu.get().readByte(regs::WY)) {
          m_YCondition = true;
        }
        m_lcdStartupLine = false;
      }
    }
  }
}

void
Ppu::handleOAMSearch()
{
  if (m_dot == 0) {
    m_objectCount = 0;
  }
  if (m_objectCount >= 10) {
    return;
  }

  if (m_dot % 2 == 1) {
    constexpr std::uint16_t oamBase = 0xFE00;
    const auto oamIndex = static_cast<std::uint8_t>(m_dot / 2);
    const auto oamAddress =
      static_cast<std::uint16_t>(oamBase + (oamIndex * 4));
    const bool is8x16Mode = (m_mmu.get().readByte(regs::LCDC) & 0x04U) != 0;
    const unsigned objectHeight = is8x16Mode ? 16 : 8;
    const auto yPos = m_mmu.get().readByte(oamAddress);
    const auto xPos = m_mmu.get().readByte(oamAddress + 1);
    const auto tileIndex = m_mmu.get().readByte(oamAddress + 2);
    const auto attributes = m_mmu.get().readByte(oamAddress + 3);
    const unsigned scanlinePlus16 = static_cast<unsigned>(m_scanline) + 16;
    if (scanlinePlus16 >= yPos && scanlinePlus16 < yPos + objectHeight) {
      m_objects.at(m_objectCount) = { oamIndex,  yPos,       xPos,
                                      tileIndex, attributes, false };
      ++m_objectCount;
    }
  }
}
bool
Ppu::handlePixelTransfer()
{
  const bool objectFetchWasActive = m_fetcher.isFetchingObject();
  m_fetcher.runNextTCycle();

  // Mesen returns from the draw cycle even on object step 5, where the high
  // byte is read and the background fetcher is restored.  Do not pop a BG
  // pixel on that completion dot.
  if (objectFetchWasActive || m_fetcher.isFetchingObject()) {
    return false;
  }

  if (m_bgWndFifo.empty()) {
    return false;
  }

  if (m_initialPipelinePixelsToDiscard > 0) {
    m_bgWndFifo.pop();
    --m_initialPipelinePixelsToDiscard;
    if (!m_objFifo.empty()) {
      m_objFifo.pop();
    }
    return false;
  }

  if (m_scxDiscardedCount < m_scx3LowBits) {
    m_bgWndFifo.pop();
    ++m_scxDiscardedCount;
    return false;
  }
  if (m_windowPixelsToDiscard > 0) {
    m_bgWndFifo.pop();
    --m_windowPixelsToDiscard;
    return false;
  }
  const auto pixelIndex =
    ((static_cast<std::size_t>(m_scanline) * SCREEN_WIDTH) + m_pixelsRendered) *
    3;

  const auto lcdc = m_mmu.get().readByte(regs::LCDC);
  constexpr std::uint8_t bgWindowEnableMask = 0x01;
  if ((lcdc & LCD_ENABLE_BIT) != 0 &&
      (lcdc & bgWindowEnableMask) == 0) {
    std::cerr << std::dec << "DMG BG-disabled FIFO pop: LY="
              << static_cast<unsigned>(m_scanline) << " dot=" << m_dot
              << " x=" << static_cast<unsigned>(m_pixelsRendered)
              << " scxLow=" << static_cast<unsigned>(m_scx3LowBits)
              << " pipelineRemaining="
              << static_cast<unsigned>(m_initialPipelinePixelsToDiscard)
              << '\n';
    //std::abort();
  }

  const auto bgPixel = m_bgWndFifo.pop();
  auto bgColorIndex = bgPixel.colorIndex;
  const bool native = m_hardwareMode == HardwareMode::CgbNative;
  if (!native && (lcdc & bgWindowEnableMask) == 0) {
    bgColorIndex = 0;
  }

  const auto bgp = m_mmu.get().readByte(regs::BGP);
  constexpr unsigned shadeMask = 0x03;
  const auto shade = static_cast<std::uint8_t>(
    (static_cast<unsigned>(bgp) >> (static_cast<unsigned>(bgColorIndex) * 2U)) &
    shadeMask);
  if (m_scanline == 0 && m_pixelsRendered < 8) {
    std::cerr << std::dec << "LY0 pixel: dot=" << m_dot
              << " x=" << static_cast<unsigned>(m_pixelsRendered)
              << " bgColor=" << static_cast<unsigned>(bgColorIndex)
              << " BGP=" << static_cast<unsigned>(bgp)
              << " shade=" << static_cast<unsigned>(shade) << '\n';
  }
  // A DMG-only cartridge running in CGB compatibility mode still computes
  // its shade index exactly as above, but looks it up in CGB background
  // palette 0 instead of the fixed DMG grayscale table - see
  // setHardwareMode()'s comment.
  constexpr std::uint8_t cgbCompatibilityBgPalette = 0;
  std::array<std::uint8_t, 3> rgb = DMG_PALETTE.at(shade);
  if (native) {
    rgb =
      cgbColorToRgb(m_mmu.get().bgPaletteColor(bgPixel.palette, bgColorIndex));
  } else if (m_hardwareMode == HardwareMode::CgbCompatibility) {
    rgb = cgbColorToRgb(
      m_mmu.get().bgPaletteColor(cgbCompatibilityBgPalette, shade));
  }

  if (!m_objFifo.empty()) {
    const auto objPixel = m_objFifo.pop();
    const auto objectBehindBackground = objPixel.behindBackground;
    constexpr std::uint8_t objEnableMask = 0x02;
    const bool objEnabled =
      (m_mmu.get().readByte(regs::LCDC) & objEnableMask) != 0;
    const bool masterPriority = (lcdc & bgWindowEnableMask) != 0;
    const bool objectAboveBackground =
      native ? (!masterPriority || bgColorIndex == 0 ||
                (!bgPixel.priority && !objectBehindBackground))
             : (bgColorIndex == 0 || !objectBehindBackground);
    if (objPixel.colorIndex != 0 && objEnabled && objectAboveBackground) {
      const auto objPaletteAddress = static_cast<std::uint16_t>(
        objPixel.palette == 0 ? regs::OBP0 : regs::OBP1);
      const auto obp = m_mmu.get().readByte(objPaletteAddress);
      const auto objShade = static_cast<std::uint8_t>(
        (static_cast<unsigned>(obp) >>
         (static_cast<unsigned>(objPixel.colorIndex) * 2U)) &
        shadeMask);
      // objPixel.palette (0 or 1, from OAM attribute bit 4 - the same bit
      // that selects OBP0/OBP1 above) doubles as the CGB object palette
      // index in compatibility mode - see setHardwareMode()'s comment.
      std::array<std::uint8_t, 3> objRgb = DMG_PALETTE.at(objShade);
      if (native) {
        objRgb = cgbColorToRgb(
          m_mmu.get().objPaletteColor(objPixel.palette, objPixel.colorIndex));
      } else if (m_hardwareMode == HardwareMode::CgbCompatibility) {
        objRgb = cgbColorToRgb(
          m_mmu.get().objPaletteColor(objPixel.palette, objShade));
      }

      rgb = objRgb;
    }
  }
  m_frameBuffer.at(pixelIndex) = rgb.at(0);
  m_frameBuffer.at(pixelIndex + 1) = rgb.at(1);
  m_frameBuffer.at(pixelIndex + 2) = rgb.at(2);
  ++m_pixelsRendered;

  return m_pixelsRendered >= SCREEN_WIDTH;
}

const Ppu::FrameBuffer&
Ppu::frameBuffer() const
{
  return m_completedFrameBuffer;
}

void
Ppu::saveFetcherState()
{
  m_savedFetcherState = m_fetcher;
}

void
Ppu::restoreFetcherState()
{
  m_fetcher = m_savedFetcherState;
}

void
Ppu::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(static_cast<std::uint8_t>(m_hardwareMode));
  for (const auto& object : m_objects) {
    object.serialize(writer);
  }
  writer.writeSize(m_objectCount);
  writer.writeU8(m_scanline);
  writer.writeU8(m_activeWindowRow);
  writer.writeU16(m_dot);
  writer.writeU8(static_cast<std::uint8_t>(m_mode));
  writer.writeBool(m_lcdEnabled);
  writer.writeBool(m_lcdStartupLine);
  writer.writeBool(m_blankFirstFrame);
  writer.writeU8(m_pixelsRendered);
  writer.writeU8(m_scx3LowBits);
  writer.writeU8(m_scxDiscardedCount);
  writer.writeU8(m_initialPipelinePixelsToDiscard);
  writer.writeU8(m_windowPixelsToDiscard);
  writer.writeBool(m_YCondition);
  m_bgWndFifo.serialize(writer);
  m_objFifo.serialize(writer);
  writer.writeBytes(m_frameBuffer);
  writer.writeBytes(m_completedFrameBuffer);
  m_fetcher.serialize(writer);
  m_savedFetcherState.serialize(writer);
}

void
Ppu::deserialize(SaveStateReader& reader)
{
  m_hardwareMode = static_cast<HardwareMode>(reader.readU8());
  for (auto& object : m_objects) {
    object.deserialize(reader);
  }
  m_objectCount = reader.readSize();
  m_scanline = reader.readU8();
  m_activeWindowRow = reader.readU8();
  m_dot = reader.readU16();
  m_mode = static_cast<Mode>(reader.readU8());
  m_lcdEnabled = reader.readBool();
  m_lcdStartupLine = reader.readBool();
  m_blankFirstFrame = reader.readBool();
  m_pixelsRendered = reader.readU8();
  m_scx3LowBits = reader.readU8();
  m_scxDiscardedCount = reader.readU8();
  m_initialPipelinePixelsToDiscard = reader.readU8();
  m_windowPixelsToDiscard = reader.readU8();
  m_YCondition = reader.readBool();
  m_bgWndFifo.deserialize(reader);
  m_objFifo.deserialize(reader);
  reader.readBytes(m_frameBuffer);
  reader.readBytes(m_completedFrameBuffer);
  m_fetcher.deserialize(reader);
  m_savedFetcherState.deserialize(reader);
}
};
