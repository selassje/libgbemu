module gbemu;

import :regs;

namespace {

constexpr std::uint8_t TOTAL_SCANLINES = 154;
constexpr std::uint8_t LAST_VISIBLE_SCANLINE = 143;
constexpr std::uint16_t MODE_2_DOTS = 80;
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

}

namespace gbemu {

void
Ppu::Fetcher::checkForObject()
{
  if (m_mode == Mode::Object) {
    return;
  }
  for (std::size_t i = 0; i < m_ppu.get().m_objectCount; ++i) {
    auto& object = m_ppu.get().m_objects.at(i);
    if (object.isFetched) {
      continue;
    }
    const auto xPos = object.xPos;
    if (m_ppu.get().m_pixelsRendered + 8 >= xPos) {
      object.isFetched = true;
      m_currentObject = object;
      m_ppu.get().saveFetcherState();
      reset(Mode::Object);
      break;
    }
  }
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
  const auto wxReached = m_ppu.get().m_pixelsRendered + 7 == wx;
  if (windowEnabled && yCondition && wxReached) {
    reset(Mode::Window);
  }
}

void
Ppu::Fetcher::runNextTCycle()
{
  // Window must be checked first: its trigger is an exact-equality check
  // on m_pixelsRendered (unlike the object trigger, which is deliberately
  // >= so it survives being stalled). If an object's fetch claimed this
  // dot first, output stalls until the fetch completes, m_pixelsRendered
  // has already moved past the window's trigger point by the time control
  // returns, and the window would never enter for this scanline. Checking
  // window first lets it switch mode (and get correctly snapshotted by the
  // object's saveFetcherState()) before an object fetch can steal the dot.
  checkForWindow();
  checkForObject();

  const auto elapsedDots = m_ppu.get().m_dot - m_lastDotStateChange;
  const auto currentState = m_mState;
  constexpr std::uint16_t tileDataBlock0 = 0x8000;
  constexpr std::uint16_t tileDataBlock1 = 0x8800;
  constexpr std::uint16_t tileDataBlock2 = 0x9000;

  const auto pushTileRowToFifo = [this]() {
    if (m_rowPushed || m_ppu.get().m_bgWndFifo.size() >= 8) {
      return false;
    }
    for (unsigned bit = 8; bit-- > 0;) {
      const auto colorIndex = static_cast<std::uint8_t>(
        (((static_cast<unsigned>(m_tileDataHigh) >> bit) & 1U) << 1U) |
        ((static_cast<unsigned>(m_tileDataLow) >> bit) & 1U));
      m_ppu.get().m_bgWndFifo.push(colorIndex);
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
    constexpr std::uint8_t paletteMask = 0x10;
    constexpr std::uint8_t priorityMask = 0x80;

    const bool xFlip = (m_currentObject.attributes & xFlipMask) != 0;
    const auto palette = static_cast<std::uint8_t>(
      (m_currentObject.attributes & paletteMask) != 0 ? 1 : 0);
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
    objFifo.merge(0, pixels, [](const auto& incoming, const auto& existing) {
      if (incoming.colorIndex == 0) {
        return false;
      }
      if (existing.colorIndex == 0) {
        return true;
      }
      return std::tie(incoming.objectX, incoming.oamIndex) <
             std::tie(existing.objectX, existing.oamIndex);
    });
  };

  switch (m_mState) {
    case State::ReadTile:
      if (elapsedDots >= 1) {

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
        m_mTileIndex = m_mmu.get().readByte(tileMapAddress);
        m_mState = State::ReadTileDataLow;
      }
      break;
    case State::ReadTileDataLow:
    case State::ReadTileDataHigh:
      if (elapsedDots >= 1) {
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
          tileByte = m_mmu.get().readByte(tileDataBlock0 + tileOffset);
        } else {
          const auto scy =
            m_mode == Mode::Window ? 0 : m_mmu.get().readByte(regs::SCY);
          const auto rowOffset = static_cast<std::uint16_t>(
            (((m_Y + scy) % 8) * 2) + (isHighByte ? 1 : 0));
          if (m_mTileIndex >= 128) {
            const auto tileOffset = static_cast<std::uint16_t>(
              ((m_mTileIndex - 128) * 16) + rowOffset);
            tileByte = m_mmu.get().readByte(tileDataBlock1 + tileOffset);
          } else {
            const auto tileOffset =
              static_cast<std::uint16_t>((m_mTileIndex * 16) + rowOffset);
            const auto lcdc = m_mmu.get().readByte(regs::LCDC);
            if ((lcdc & 0x10U) != 0) {
              tileByte = m_mmu.get().readByte(tileDataBlock0 + tileOffset);
            } else {
              tileByte = m_mmu.get().readByte(tileDataBlock2 + tileOffset);
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
      if (elapsedDots >= 1) {
        m_mState = State::PushToFifo;
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
    m_ppu.get().m_scxDiscardedCount = m_ppu.get().m_scx3LowBits;
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
      m_dot = 0;
      m_scanline = 0;
      m_mode = Mode::HBlank;
      m_pixelsRendered = 0;
      m_activeWindowRow = 0;
      m_YCondition = false;
      m_bgWndFifo.clear();
      m_objFifo.clear();
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
    // scanline 0/mode 2 - never resumes whatever mode/scanline it was
    // paused at before being disabled.
    m_lcdEnabled = true;
    m_dot = 0;
    m_scanline = 0;
    m_mode = Mode::OAMSearch;
    m_mmu.get().writeByte(regs::LY, m_scanline);
    m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
    m_mmu.get().updateStatCoincidence(m_scanline ==
                                      m_mmu.get().readByte(regs::LYC));
  }

  switch (m_mode) {
    case Mode::HBlank:
    case Mode::VBlank:
      // Both are genuinely idle time on real hardware - no fetcher/FIFO
      // activity, nothing to render. Mode transitions, LY updates, and the
      // VBlank interrupt are all already handled by incrementDot().
      break;
    case Mode::OAMSearch:
      handleOAMSearch();
      break;
    case Mode::PixelTransfer:
      if (handlePixelTransfer()) {
        m_mode = Mode::HBlank;
        m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
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
      }
    } else {
      m_mode = Mode::OAMSearch;
      m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
    }
  } else {
    if (m_scanline <= LAST_VISIBLE_SCANLINE) {
      if (m_dot == MODE_2_DOTS) {
        m_mode = Mode::PixelTransfer;
        m_mmu.get().updateStatMode(static_cast<std::uint8_t>(m_mode));
        constexpr std::uint8_t scxLow3BitsMask = 0x07;
        m_scx3LowBits = static_cast<std::uint8_t>(
          m_mmu.get().readByte(regs::SCX) & scxLow3BitsMask);
        m_scxDiscardedCount = 0;
        m_objFifo.clear();
        m_fetcher.reset(Fetcher::Mode::Background);
        if (!m_YCondition && m_scanline == m_mmu.get().readByte(regs::WY)) {
          m_YCondition = true;
        }
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
  m_fetcher.runNextTCycle();

  if (m_fetcher.isFetchingObject()) {
    return false;
  }

  if (m_bgWndFifo.empty()) {
    return false;
  }

  if (m_scxDiscardedCount < m_scx3LowBits) {
    m_bgWndFifo.pop();
    ++m_scxDiscardedCount;
    return false;
  }
  const auto pixelIndex =
    ((static_cast<std::size_t>(m_scanline) * SCREEN_WIDTH) + m_pixelsRendered) *
    3;

  auto bgColorIndex = m_bgWndFifo.pop();
  constexpr std::uint8_t bgWindowEnableMask = 0x01;
  if ((m_mmu.get().readByte(regs::LCDC) & bgWindowEnableMask) == 0) {
    bgColorIndex = 0;
  }

  const auto bgp = m_mmu.get().readByte(regs::BGP);
  constexpr unsigned shadeMask = 0x03;
  const auto shade = static_cast<std::uint8_t>(
    (static_cast<unsigned>(bgp) >> (static_cast<unsigned>(bgColorIndex) * 2U)) &
    shadeMask);
  auto rgb = DMG_PALETTE.at(shade);

  if (!m_objFifo.empty()) {
    const auto objPixel = m_objFifo.pop();
    const auto objectBehindBackground = objPixel.behindBackground;
    constexpr std::uint8_t objEnableMask = 0x02;
    const bool objEnabled =
      (m_mmu.get().readByte(regs::LCDC) & objEnableMask) != 0;
    if (objPixel.colorIndex != 0 && objEnabled &&
        (bgColorIndex == 0 || !objectBehindBackground)) {
      const auto objPaletteAddress = static_cast<std::uint16_t>(
        objPixel.palette == 0 ? regs::OBP0 : regs::OBP1);
      const auto obp = m_mmu.get().readByte(objPaletteAddress);
      const auto objShade = static_cast<std::uint8_t>(
        (static_cast<unsigned>(obp) >>
         (static_cast<unsigned>(objPixel.colorIndex) * 2U)) &
        shadeMask);
      const auto objRgb = DMG_PALETTE.at(objShade);

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
  return m_frameBuffer;
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
};
