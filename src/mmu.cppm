export module gbemu:mmu;

import std;

namespace gbemu {

export inline constexpr std::size_t MIN_ROM_SIZE = 0x150;

// Values match JOYP's bit-position convention: Right/Left/Up/Down occupy
// bits 0-3 of the directional-key group, A/B/Select/Start occupy bits 0-3
// of the button-key group - stored here shifted into one uint8_t (buttons
// in the upper nibble) so both groups fit in a single m_buttonState byte.
export enum class Button : std::uint8_t {
  Right,
  Left,
  Up,
  Down,
  A,
  B,
  Select,
  Start
};

#ifdef ENABLE_TESTS
export inline std::string
  gSerialOutput; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
// Some test ROMs (e.g. blargg's interrupt_time.gb) report their result via a
// zero-terminated string written to cartridge RAM at $A004 instead of over
// the serial port. Captured positionally (indexed by address, not
// append-on-write) so the interleaved null-terminator writes each character
// print performs land in the right place.
export inline std::string
  gMemoryOutput; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
#endif

class Mmu // NOLINT(misc-use-internal-linkage)
{
public:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t KB8 = 0x2000;
  static constexpr std::size_t KB4 = 0x1000;

  Mmu() = default;

  [[nodiscard]] std::uint8_t readByte(std::uint16_t address) const;
  [[nodiscard]] std::uint16_t readWord(std::uint16_t address) const;
  void writeByte(std::uint16_t address, std::uint8_t value);
  void writeWord(std::uint16_t address, std::uint16_t value);

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  void enableBootRom(std::span<const std::uint8_t> bootRom);

  // PPU-only: bypass the CPU-facing write mask on STAT's read-only bits
  // (0-2). Not reachable through writeByte(), same reasoning as
  // enableBootRom() being a dedicated method rather than a writeByte()
  // special case.
  void updateStatMode(std::uint8_t mode);
  void updateStatCoincidence(bool coincidence);

  // Frontend-facing: records real button state, combined with whichever
  // group(s) JOYP currently selects at read time (see readByte()).
  void setButtonState(Button button, bool pressed);

  // Advances an in-progress OAM DMA transfer (if any) by one T-cycle.
  // Mirrors Ppu::runNextTCycle() - called once per T-cycle from
  // GameBoy::runNextFrame()'s loop.
  void runNextTCycle();

private:
  // Real hardware copies 1 byte per 4 T-cycles (160 bytes -> 640 T-cycles
  // total), not all 160 at once - std::nullopt when no transfer is active.
  struct DmaState
  {
    std::uint16_t sourceBase{ 0 };
    std::uint8_t offset{ 0 };
    std::uint8_t tCyclesSinceLastByte{ 0 };
  };
  std::optional<DmaState> m_dmaState;

  std::vector<std::uint8_t> m_bootRom;
  bool m_bootRomActive{ false };
  std::vector<std::uint8_t> m_rom;
  std::array<std::uint8_t, KB16> m_vram{};
  std::array<std::uint8_t, KB8> m_extRam{};
  std::array<std::uint8_t, KB4 * 8> m_wram{};
  std::array<std::uint8_t, 0xA0> m_oam{};  // NOLINT(readability-magic-numbers)
  std::array<std::uint8_t, 0x80> m_io{};   // NOLINT(readability-magic-numbers)
  std::array<std::uint8_t, 0x7F> m_hram{}; // NOLINT(readability-magic-numbers)
  std::size_t m_switchableRomBank{ 1 };
  std::uint8_t m_mbc1RomBankLow{ 1 };
  std::uint8_t m_mbc1BankHigh{ 0 };
  bool m_mbc1BankingMode{ false };
  bool m_usesMbc1{ false };
  std::size_t m_switchableVRamBank{ 0 };
  std::size_t m_switchableWRamBank{ 1 };
  std::uint8_t m_interruptEnableRegister{ 0 };
  std::uint8_t m_unusable{ 0 };
  // 1 = pressed, one bit per Button - directional keys in bits 0-3, button
  // keys in bits 4-7 (matching Button's own enumerator order/values).
  std::uint8_t m_buttonState{ 0 };

  [[nodiscard]] std::uint8_t& getByteRef(std::uint16_t address);
};

}
