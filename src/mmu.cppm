export module gbemu:mmu;

import std;
import :apu;

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
// Function-local statics, not plain exported globals: sidesteps a
// reproducible clang codegen crash (in this toolchain's experimental
// snapshot) in the llvm.global_ctors list machinery that non-trivially-
// initialized module-exported globals need - a local static instead uses
// the unrelated (C++11 thread-safe) guarded-initialization codegen path.
export std::string&
serialOutput();
// Some test ROMs (e.g. blargg's interrupt_time.gb) report their result via a
// zero-terminated string written to cartridge RAM at $A004 instead of over
// the serial port. Captured positionally (indexed by address, not
// append-on-write) so the interleaved null-terminator writes each character
// print performs land in the right place.
export std::string&
memoryOutput();
#endif

class Mmu // NOLINT(misc-use-internal-linkage)
{
public:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t KB8 = 0x2000;
  static constexpr std::size_t KB4 = 0x1000;

  // Forwards channel-register writes (NR10-NR44) to apu - see
  // writeByte(). apu must outlive this Mmu.
  explicit Mmu(Apu& apu)
    : m_apu(apu)
  {
  }

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

  // The real 16-bit free-running divider counter DIV (0xFF04) is just the
  // upper 8 bits of - incremented every T-cycle, reset (in full, not just
  // the CPU-visible byte) by a write to DIV, see writeByte(). Exposed for
  // Apu's frame sequencer (and eventually Cpu's timer handling) to watch
  // specific bits of directly, bypassing readByte()'s 8-bit CPU-facing
  // view - those subsystems tap bits below DIV's own visible range.
  [[nodiscard]] std::uint16_t divCounter() const { return m_divCounter; }

private:
  std::reference_wrapper<Apu> m_apu;

  // Real hardware copies 1 byte per 4 T-cycles (160 bytes -> 640 T-cycles
  // total), not all 160 at once - std::nullopt when no transfer is active.
  struct DmaState
  {
    std::uint16_t sourceBase{ 0 };
    std::uint8_t offset{ 0 };
    std::uint8_t tCyclesSinceLastByte{ 0 };
  };
  std::optional<DmaState> m_dmaState;

  // An internally-clocked transfer (SC bit 0 set) completes on its own
  // fixed timing regardless of whether a real link partner is present -
  // real hardware shifts 8 bits at ~8192 Hz (512 T-cycles/bit), so 4096
  // T-cycles for the whole byte. An externally-clocked transfer (bit 0
  // clear) has no local timer driving it and is deliberately left to just
  // sit there forever with nothing to advance it - that's genuinely
  // correct behavior with no partner connected, not a bug.
  std::optional<std::uint16_t> m_serialTCyclesRemaining;

  // See divCounter(). Wraps naturally on overflow (plain unsigned
  // arithmetic) - exactly the free-running behavior real hardware has.
  std::uint16_t m_divCounter{ 0 };

  std::vector<std::uint8_t> m_bootRom;
  bool m_bootRomActive{ false };
  std::vector<std::uint8_t> m_rom;
  std::array<std::uint8_t, KB16> m_vram{};
  std::array<std::uint8_t, KB8> m_extRam{};
  std::array<std::uint8_t, KB4 * 8> m_wram{};
  std::array<std::uint8_t, 0xA0> m_oam{};
  std::array<std::uint8_t, 0x80> m_io{};
  std::array<std::uint8_t, 0x7F> m_hram{};
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
  // Mirrors NR52 bit 7 (APU power). Starts true since m_io - and so NR52 -
  // starts zeroed, matching real hardware's power-on-reset state before the
  // boot ROM writes NR52=0x80 to turn the APU on. While true, writes to
  // NR10-NR51 (0xFF10-0xFF25) are dropped - see writeByte(). Wave RAM
  // (0xFF30-0xFF3F) and NR52 itself are unaffected either way.
  bool m_apuRegistersReadOnly{ true };

  [[nodiscard]] std::uint8_t& getByteRef(std::uint16_t address);
};

}
