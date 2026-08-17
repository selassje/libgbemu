export module gbemu:mmu;

import std;
import :apu;
import :hardware_mode;
import :mappers;
import :serialization;

namespace gbemu {

export inline constexpr std::size_t MIN_ROM_SIZE = 0x150;
// Real Game Boy ROMs are always sized in exact multiples of the fixed 16KB
// bank-0 size - every mapper's bank-count math (romSize() / KB16) assumes
// it, so loadRom() enforces it.
export inline constexpr std::size_t ROM_BANK_SIZE = 0x4000;

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
export std::string&
serialOutput();
export std::string&
memoryOutput();
#endif

class Mmu // NOLINT(misc-use-internal-linkage)
{
public:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t KB8 = 0x2000;
  static constexpr std::size_t KB4 = 0x1000;

  explicit Mmu(Apu& apu)
    : m_apu(apu)
  {
  }
  ~Mmu() = default;

  Mmu(const Mmu&) = delete;
  Mmu& operator=(const Mmu&) = delete;
  Mmu(Mmu&&) = delete;
  Mmu& operator=(Mmu&&) = delete;

  [[nodiscard]] std::uint8_t readByte(std::uint16_t address) const;
  [[nodiscard]] std::uint16_t readWord(std::uint16_t address) const;
  void writeByte(std::uint16_t address, std::uint8_t value);
  void writeWord(std::uint16_t address, std::uint16_t value);

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  void enableBootRom(std::span<const std::uint8_t> bootRom);

  void updateStatMode(std::uint8_t mode, bool triggerInterrupt);
  void triggerStatOamInterrupt();
  void updateStatCoincidence(bool coincidence);

  void setButtonState(Button button, bool pressed);

  // Advances CPU-clocked MMU state (DIV, DMA and serial) by one CPU T-cycle.
  void runNextTCycle();
  void runTimerTo(std::uint16_t divCounter);

  // DIV (0xFF04) is the upper 8 bits of a real 16-bit free-running divider
  // counter, incremented every T-cycle and reset in full by a write to
  // DIV. Exposed so Apu's frame sequencer can watch specific bits below
  // DIV's own visible range.
  [[nodiscard]] std::uint16_t divCounter() const { return m_divCounter; }

  [[nodiscard]] bool doubleSpeed() const { return m_doubleSpeed; }
  // Performs a prepared CGB speed switch and resets DIV, as STOP does on
  // hardware. Returns false when no switch was prepared or on DMG.
  bool switchSpeed();

  // Real DMG hardware doesn't have VBK/SVBK (VRAM/WRAM bank select) or
  // BCPS/BCPD/OCPS/OCPD (palette RAM) at all - reads $FF, writes are
  // no-ops - so writeByte()/readByte() only let them take effect when
  // this isn't Dmg.
  void setHardwareMode(HardwareMode mode)
  {
    m_isCgbHardware = mode != HardwareMode::Dmg;
  }

  // The CGB color (15-bit RGB555, packed 0bBBBBBGGGGGRRRRR in the low 15
  // bits) BCPS/BCPD (background) or OCPS/OCPD (object) have stored for the
  // given palette (0-7) and color-within-palette (0-3).
  [[nodiscard]] std::uint16_t bgPaletteColor(std::uint8_t palette,
                                             std::uint8_t colorIndex) const;
  [[nodiscard]] std::uint16_t objPaletteColor(std::uint8_t palette,
                                              std::uint8_t colorIndex) const;

  // Reads VRAM (0x8000-0x9FFF) from an explicitly-chosen bank, ignoring
  // whatever the CPU currently has VBK pointed at - the PPU's fetch logic
  // needs a specific bank per purpose (bank 0 for tile map indices/pixel
  // data; bank 1 only for the CGB tile-attribute byte at the same
  // tile-map address, or for pixel data when that attribute's bank bit
  // says so) regardless of what the CPU last left VBK as.
  [[nodiscard]] std::uint8_t readVram(std::uint8_t bank,
                                      std::uint16_t address) const;

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

  [[nodiscard]] bool isCgbGdmaActive() const
  {
    return m_isCgbHardware && m_cgbDmaState.bytesRemaining > 0 &&
           !m_cgbDmaState.isHDMA;
  }

  [[nodiscard]] bool isCgbHdmaActive() const
  {
    return m_isCgbHardware && m_cgbDmaState.bytesRemaining > 0 &&
           m_cgbDmaState.isHDMA && m_cgbDmaState.isHDMABlockInTransfer;
  }

  void notifyHBlankStart()
  {
    if (m_isCgbHardware && m_cgbDmaState.isHDMA &&
        m_cgbDmaState.bytesRemaining > 0) {
      m_cgbDmaState.isHDMABlockInTransfer = true;
    }
  }

private:
  std::reference_wrapper<Apu> m_apu;
  bool m_isCgbHardware{ false };
  bool m_doubleSpeed{ false };
  bool m_speedSwitchPrepared{ false };

  // Real hardware copies 1 byte per 4 T-cycles (160 bytes -> 640 T-cycles
  // total), not all 160 at once - std::nullopt when no transfer is active.
  struct DmaState
  {
    std::uint16_t sourceBase{ 0 };
    std::uint8_t offset{ 0 };
    std::uint8_t tCyclesSinceLastByte{ 0 };
  };
  std::optional<DmaState> m_dmaState;

  struct CgbDmaState
  {
    std::uint8_t sourceLow{ 0 };
    std::uint8_t sourceHigh{ 0 };
    std::uint8_t destLow{ 0 };
    std::uint8_t destHigh{ 0 };
    std::uint16_t bytesRemaining{ 0 };
    std::uint8_t tCyclesSinceLastByte{ 0 };
    bool isHDMA{ false };
    bool isHDMABlockInTransfer{ false };

    [[nodiscard]] std::uint16_t sourceAddress() const
    {
      return static_cast<std::uint16_t>(
        (static_cast<unsigned>(sourceHigh) << 8U) | sourceLow);
    }

    // destHigh/destLow store a 13-bit offset within VRAM - real hardware
    // forces the destination's top 3 address bits to 0b100 regardless of
    // what's written, so VRAM's 0x8000 base is added back in here.
    [[nodiscard]] std::uint16_t destAddress() const
    {
      constexpr std::uint16_t vramBase = 0x8000;
      return static_cast<std::uint16_t>(
        vramBase | (static_cast<unsigned>(destHigh) << 8U) | destLow);
    }
  };

  CgbDmaState m_cgbDmaState;
  // An internally-clocked transfer (SC bit 0 set) completes on its own
  // fixed timing regardless of whether a real link partner is present -
  // real hardware shifts 8 bits at ~8192 Hz (512 T-cycles/bit), so 4096
  // T-cycles for the whole byte. An externally-clocked transfer (bit 0
  // clear) has no local timer driving it and is deliberately left to just
  // sit there forever - genuinely correct behavior with no partner
  // connected, not a bug.
  std::optional<std::uint16_t> m_serialTCyclesRemaining;

  std::uint16_t m_divCounter{ 0 };
  std::uint16_t m_timerDivCounter{ 0 };
  std::uint8_t m_timerReloadTCycles{ 0 };
  bool m_timerReloadedThisCycle{ false };

  [[nodiscard]] bool timerInput() const;
  void incrementTimer();

  std::vector<std::uint8_t> m_bootRom;
  bool m_bootRomActive{ false };
  MapperVariant m_mapper;
  std::array<std::uint8_t, KB16> m_vram{};
  std::array<std::uint8_t, KB4 * 8> m_wram{};
  std::array<std::uint8_t, 0xA0> m_oam{};
  std::array<std::uint8_t, 0x80> m_io{};
  std::array<std::uint8_t, 0x7F> m_hram{};
  std::size_t m_switchableVRamBank{ 0 };
  std::size_t m_switchableWRamBank{ 1 };
  // 8 palettes x 4 colors x 2 bytes/color (15-bit RGB555, little-endian).
  std::array<std::uint8_t, 64> m_bgPaletteRam{};
  std::array<std::uint8_t, 64> m_objPaletteRam{};
  std::uint8_t m_interruptEnableRegister{ 0 };
  std::uint8_t m_unusable{ 0 };
  // 1 = pressed, one bit per Button - directional keys in bits 0-3, button
  // keys in bits 4-7.
  std::uint8_t m_buttonState{ 0 };
  // Mirrors NR52 bit 7 (APU power). Starts true, matching real hardware's
  // power-on-reset state before the boot ROM writes NR52=0x80. While true,
  // writes to NR10-NR51 (0xFF10-0xFF25) are dropped - Wave RAM
  // (0xFF30-0xFF3F) and NR52 itself are unaffected.
  bool m_apuRegistersReadOnly{ true };

  template<typename Self>
  [[nodiscard]] auto& getByteRef(this Self& self, std::uint16_t address);
};

}
