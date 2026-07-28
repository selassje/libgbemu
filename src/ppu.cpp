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
Ppu::Fifo::push(std::uint8_t pixel)
{
  m_buffer.at((m_head + m_size) % m_buffer.size()) = pixel;
  ++m_size;
}

std::uint8_t
Ppu::Fifo::pop()
{
  const auto pixel = m_buffer.at(m_head);
  m_head = (m_head + 1) % m_buffer.size();
  --m_size;
  return pixel;
}

void
Ppu::Fetcher::checkForWindow()
{
  if (m_mode == Mode::Window) {
    return;
  }
  const auto wx = m_mmu.get().readByte(regs::WX);
  const auto windowEnabled = (m_mmu.get().readByte(regs::LCDC) & 0x20U) !=
                             0; // NOLINT(readability-magic-numbers)
  const auto yCondition = m_ppu.get().m_YCondition;
  const auto wxReached =
    m_ppu.get().m_pixelsRendered + 7 == wx; // NOLINT(readability-magic-numbers)
  if (windowEnabled && yCondition && wxReached) {
    reset(Mode::Window);
  }
}

void
Ppu::Fetcher::runNextTCycle()
{
  checkForWindow();

  const auto elapsedDots = m_ppu.get().m_dot - m_lastDotStateChange;
  const auto currentState = m_mState;
  constexpr std::uint16_t tileDataBlock0 =
    0x8000; // NOLINT(readability-magic-numbers)
  constexpr std::uint16_t tileDataBlock1 =
    0x8800; // NOLINT(readability-magic-numbers) ]
  constexpr std::uint16_t tileDataBlock2 =
    0x9000; // NOLINT(readability-magic-numbers) ]

  const auto pushTileRowToFifo = [this]() {
    if (m_rowPushed || m_ppu.get().m_bgWndFifo.size() >=
                         8) { // NOLINT(readability-magic-numbers)
      return false;
    }
    for (unsigned bit = 8; bit-- > 0;) { // NOLINT(readability-magic-numbers)
      const auto colorIndex = static_cast<std::uint8_t>(
        (((static_cast<unsigned>(m_tileDataHigh) >> bit) & 1U) << 1U) |
        ((static_cast<unsigned>(m_tileDataLow) >> bit) & 1U));
      m_ppu.get().m_bgWndFifo.push(colorIndex);
    }
    m_rowPushed = true;
    return true;
  };

  switch (m_mState) {
    case State::ReadTile:
      if (elapsedDots >= 1) {

        const auto lcdc = m_mmu.get().readByte(regs::LCDC);
        const auto lcdcBit3 =
          (lcdc & 0x08U) != 0; // NOLINT(readability-magic-numbers)
        const auto lcdcBit6 =
          (lcdc & 0x40U) != 0; // NOLINT(readability-magic-numbers)
        const auto isWindow = (m_mode == Mode::Window);
        const std::uint16_t tileMapBaseAddress = [&]() -> std::uint16_t {
          if ((isWindow && lcdcBit6) || (!isWindow && lcdcBit3)) {
            return 0x9C00; // NOLINT(readability-magic-numbers)
          }
          return 0x9800; // NOLINT(readability-magic-numbers)
        }();

        const auto& [tileX,
                     tileY] = [&]() -> std::pair<std::uint8_t, std::uint8_t> {
          if (m_mode == Mode::Window) {
            return { m_tileX % 32, m_Y % 256 };
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
        const auto scy =
          m_mode == Mode::Window ? 0 : m_mmu.get().readByte(regs::SCY);
        const auto rowOffset = static_cast<std::uint16_t>(
          (((m_Y + scy) % 8) * 2) + (isHighByte ? 1 : 0));
        std::uint8_t tileByte{};
        if (m_mTileIndex >= 128) {
          const auto tileOffset =
            static_cast<std::uint16_t>(((m_mTileIndex - 128) * 16) + rowOffset);
          tileByte = m_mmu.get().readByte(tileDataBlock1 + tileOffset);
        } else {
          const auto tileOffset =
            static_cast<std::uint16_t>((m_mTileIndex * 16) + rowOffset);
          const auto lcdc = m_mmu.get().readByte(regs::LCDC);
          if ((lcdc & 0x10U) != 0) { // NOLINT(readability-magic-numbers)
            tileByte = m_mmu.get().readByte(tileDataBlock0 + tileOffset);
          } else {
            tileByte = m_mmu.get().readByte(tileDataBlock2 + tileOffset);
          }
        }
        if (isHighByte) {
          m_tileDataHigh = tileByte;
          m_mState = State::Sleep;
          pushTileRowToFifo();
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
  m_ppu.get().m_bgWndFifo.clear();
}

void
Ppu::runNextTCycle()
{
  // Real hardware: the PPU is completely inert while LCDC bit 7 is clear -
  // no dot/scanline/mode advancement, no interrupts. LY stays wherever it
  // was left (0 at boot, since the CPU/PPU/MMU all reset to zero) until the
  // boot ROM (or a game) explicitly enables the LCD.
  if ((m_mmu.get().readByte(regs::LCDC) & LCD_ENABLE_BIT) == 0) {
    return;
  }

  switch (m_mode) {
    case Mode::HBlank:
      handleHBlank();
      break;
    case Mode::VBlank:
      handleVBlank();
      break;
    case Mode::OAMSearch:
      handleOAMSearch();
      break;
    case Mode::PixelTransfer:
      if (handlePixelTransfer()) {
        m_mode = Mode::HBlank;
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
    if (m_scanline > LAST_VISIBLE_SCANLINE) {
      m_mode = Mode::VBlank;
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
    }
  } else {
    if (m_scanline <= LAST_VISIBLE_SCANLINE) {
      if (m_dot == MODE_2_DOTS) {
        m_mode = Mode::PixelTransfer;
        constexpr std::uint8_t scxLow3BitsMask = 0x07;
        m_scx3LowBits = static_cast<std::uint8_t>(
          m_mmu.get().readByte(regs::SCX) & scxLow3BitsMask);
        m_scxDiscardedCount = 0;
        m_fetcher.reset(Fetcher::Mode::Background);
        if (!m_YCondition && m_scanline == m_mmu.get().readByte(regs::WY)) {
          m_YCondition = true;
        }
      }
    }
  }
}

void
Ppu::handleHBlank() {
  // HBlank logic can be implemented here

};

void
Ppu::handleVBlank() {
  // VBlank logic can be implemented here

};

void
Ppu::handleOAMSearch() {
  // OAM Search logic can be implemented here

};

bool
Ppu::handlePixelTransfer()
{
  m_fetcher.runNextTCycle();

  if (m_bgWndFifo.empty()) {
    return false;
  }

  if (m_scxDiscardedCount < m_scx3LowBits) {
    m_bgWndFifo.pop();
    ++m_scxDiscardedCount;
    return false;
  }

  const auto colorIndex = m_bgWndFifo.pop();
  const auto bgp = m_mmu.get().readByte(regs::BGP);
  constexpr unsigned shadeMask = 0x03;
  const auto shade = static_cast<std::uint8_t>(
    (static_cast<unsigned>(bgp) >> (static_cast<unsigned>(colorIndex) * 2U)) &
    shadeMask);
  const auto& rgb = DMG_PALETTE.at(shade);
  const auto pixelIndex =
    ((static_cast<std::size_t>(m_scanline) * SCREEN_WIDTH) + m_pixelsRendered) *
    3;
  m_frameBuffer.at(pixelIndex) = rgb.at(0);
  m_frameBuffer.at(pixelIndex + 1) = rgb.at(1);
  m_frameBuffer.at(pixelIndex + 2) = rgb.at(2);
  ++m_pixelsRendered;

  return m_pixelsRendered >= SCREEN_WIDTH;
};

Ppu::FrameBuffer&
Ppu::frameBuffer()
{
  return m_frameBuffer;
}
};
