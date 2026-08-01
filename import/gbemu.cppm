export module gbemu;

import std;

export import :apu;
export import :cpu;
export import :mmu;
export import :ppu;
export import :boot_rom;
export import :regs;
// Structurally required (an interface partition must be reachable from
// the primary module interface), but the HardwareMode enum it declares
// isn't itself marked `export` - see its own comment for why that keeps
// it out of this module's public API despite this line.
export import :hardware_mode;

export namespace gbemu {

struct Rgb
{
  std::uint8_t r;
  std::uint8_t g;
  std::uint8_t b;
};

struct EmulationFrame
{
  std::mdspan<const std::uint8_t,
              std::extents<std::size_t, SCREEN_HEIGHT, SCREEN_WIDTH, 3>>
    pixels;
  // Interleaved stereo (L, R, L, R, ...), normalized to [-1, 1] - dynamic
  // in the sample-frame count (the Game Boy's clock doesn't divide evenly
  // into any standard sample rate, so this varies by a sample or two frame
  // to frame), fixed at 2 channels.
  std::mdspan<const float, std::extents<std::size_t, std::dynamic_extent, 2>>
    audio;
};

// Which physical console this GameBoy instance emulates - a property of
// the console itself, not of whatever cartridge happens to be inserted.
enum class Mode : std::uint8_t
{
  // Boots as DMG for a cartridge that doesn't declare CGB support/
  // requirement (header byte 0x0143), or as CGB for one that does -
  // matches inserting a cartridge into whichever real hardware it was
  // actually designed for. This is a policy for resolving *this*
  // emulator's boot choice, not a real hardware mode in its own right.
  Auto,
  // Always boots as DMG, regardless of what the cartridge declares -
  // matches inserting a cartridge into a real DMG console. Rejected with
  // an error (see initializeFromRom()) for a cartridge whose header
  // declares CGB as *required* (0x0143 bits 7+6 both set, i.e. 0xC0-0xFF)
  // rather than merely supported, since no real DMG console could run
  // that cartridge correctly either.
  Dmg,
  // Always boots as CGB hardware, regardless of what the cartridge
  // declares - matches inserting any cartridge into a real CGB console,
  // which runs its own one fixed boot ROM either way. Lands in CGB
  // compatibility mode for a DMG-only cartridge, or native CGB mode for
  // a cartridge that itself declares CGB support.
  Cgb,
};

class GameBoy
{
public:
  // Delegates to the explicit-model constructor below instead of giving
  // it a default argument (disallowed by this project's .clang-tidy -
  // fuchsia-default-arguments-declarations).
  GameBoy()
    : GameBoy(Mode::Auto)
  {
  }

  explicit GameBoy(Mode model)
    : m_model(model)
    , m_mmu(m_apu)
    , m_ppu(m_mmu)
    , m_cpu(m_mmu, m_ppu, m_apu)
  {
  }

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  // Power-cycle equivalent: re-runs the exact same boot sequence against
  // the already-loaded cartridge, without needing it re-supplied - the
  // same cartridge stays "inserted", same as a real Game Boy's power
  // switch off/on.
  [[nodiscard]] std::expected<void, std::string> reset();

  std::expected<EmulationFrame, std::string> runNextFrame();
  void setButtonState(Button button, bool pressed);

private:
  // Reconstructs Mmu/Ppu/Cpu from scratch (guaranteeing every field
  // returns to its true declared default, rather than a hand-maintained
  // per-field reset that could silently miss one) and re-runs the same
  // load sequence loadRom()/reset() both need, operating on whatever's
  // currently in m_romBytes.
  [[nodiscard]] std::expected<void, std::string> initializeFromRom();

  // Caller-requested console model - see Mode's own comment.
  // Declared first since the constructor's init list initializes it
  // first (it doesn't depend on, or get depended on by, anything else
  // constructed below).
  Mode m_model;
  // Declared before m_mmu so it's fully constructed before Mmu's
  // constructor receives a reference to it (Mmu forwards channel-register
  // writes to it - see Mmu::writeByte()).
  Apu m_apu;
  Mmu m_mmu;
  // Declared before m_cpu so it's fully constructed before Cpu's
  // constructor receives a reference to it.
  Ppu m_ppu;
  Cpu m_cpu;
  // The actual, resolved hardware behavior this session boots as -
  // computed once in initializeFromRom() by combining m_model with the
  // cartridge's own header CGB flag, and handed to Apu/Mmu/Ppu via their
  // own setHardwareMode(). See HardwareMode's own comment for why this is
  // a distinct concept from m_model.
  HardwareMode m_hardwareMode{ HardwareMode::Dmg };
  // Kept so reset() can re-run initializeFromRom() without the caller
  // needing to re-supply the same ROM bytes.
  std::vector<std::uint8_t> m_romBytes;
};

}
