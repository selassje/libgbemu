module gbemu;

namespace {

constexpr std::uint8_t TOTAL_SCANLINES = 154;
constexpr std::uint8_t LAST_VISIBLE_SCANLINE = 143;
constexpr std::uint16_t MODE_2_DOTS = 80;
constexpr std::uint16_t MODE_3_DOTS = 172;
constexpr std::uint16_t MODE_0_DOTS = 456 - MODE_2_DOTS - MODE_3_DOTS;
constexpr std::uint16_t DOTS_PER_SCANLINE = 456;

constexpr std::uint16_t SCX_REGISTER_ADDRESS = 0xFF43;
constexpr std::uint16_t SCY_REGISTER_ADDRESS = 0xFF42;
constexpr std::uint16_t LY_REGISTER_ADDRESS = 0xFF44;

}

namespace gbemu {

void
Ppu::runNextTCycle()
{
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
    m_mmu.get().writeByte(LY_REGISTER_ADDRESS, m_scanline);
    if (m_scanline > LAST_VISIBLE_SCANLINE) {
      m_mode = Mode::VBlank;
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
