module gbemu;

namespace {

constexpr std::uint8_t TOTAL_SCANLINES = 154;
constexpr std::uint8_t LAST_VISIBLE_SCANLINE = 143;
constexpr std::uint16_t MODE_2_DOTS = 80;
constexpr std::uint16_t MODE_3_DOTS = 172;
constexpr std::uint16_t DOTS_PER_SCANLINE = 456;

constexpr std::uint8_t VBLANK_INTERRUPT_BIT = 0x01;
constexpr std::uint8_t FIRST_VBLANK_SCANLINE = LAST_VISIBLE_SCANLINE + 1;
constexpr std::uint8_t LCD_ENABLE_BIT = 0x80;

}

namespace gbemu {

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
      handlePixelTransfer();
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
    m_nextPixelXToRender = 0;
    m_scanline = static_cast<std::uint8_t>(m_scanline + 1) % TOTAL_SCANLINES;
    m_mmu.get().writeByte(regs::LY, m_scanline);
    if (m_scanline > LAST_VISIBLE_SCANLINE) {
      m_mode = Mode::VBlank;
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
      } else if (m_dot == MODE_2_DOTS + MODE_3_DOTS) {
        m_mode = Mode::HBlank;
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

void
Ppu::handlePixelTransfer() {
  // Pixel Transfer logic can be implemented here

};

};
