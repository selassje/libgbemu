module gbemu;

namespace {

constexpr std::uint8_t TOTAL_SCANLINES = 154;
constexpr std::uint8_t LAST_VISIBLE_SCANLINE = 143;
constexpr std::uint16_t MODE_2_DOTS = 80;
constexpr std::uint16_t MODE_3_DOTS = 172;
constexpr std::uint16_t MODE_0_DOTS = 456 - MODE_2_DOTS - MODE_3_DOTS;
constexpr std::uint16_t DOTS_PER_SCANLINE = 456;

}

namespace gbemu {

void
Ppu::runNextTCycle()
{

  ++m_dot;
  if (m_dot >= DOTS_PER_SCANLINE) {
    m_dot = 0;
    m_scanline = static_cast<std::uint8_t>(m_scanline + 1) % TOTAL_SCANLINES;
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
};