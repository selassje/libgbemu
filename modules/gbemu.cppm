export module gbemu;

import std;

export import :apu;
export import :cpu;
export import :mmu;
export import :ppu;
export import :boot_rom;
export import :regs;
export import :serialization;
// Structurally required (an interface partition must be reachable from
// the primary module interface), but the HardwareMode enum it declares
// isn't itself marked `export` - see its own comment for why that keeps
// it out of this module's public API despite this line.
export import :hardware_mode;
// Same reasoning as :hardware_mode above - none of Mapper/RomOnlyMapper/
// Mbc1Mapper/MapperVariant are marked `export` either (see mapper.cppm's
// own comments); Mmu is their only consumer.
export import :mappers;
// Same reasoning as :hardware_mode above - hardAssert() isn't marked
// `export` either (see its own comment).
export import :hard_assert;

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
  // switch off/on. Unconditionally succeeds: the only two ways
  // initializeFromRom() can reject a model+ROM pair (a too-small/malformed
  // ROM, or an incompatible Mode::Dmg forced onto a CGB-required
  // cartridge) are both validated *before* being committed to m_romBytes/
  // m_model, by loadRom()/setMode() respectively - by the time reset() can
  // be called at all, the pair it re-derives from is already known-good.
  void reset();

  // Changes which physical console this instance emulates, then reset()s -
  // matches unplugging the already-inserted cartridge, plugging it into a
  // different physical console, and powering that on. Validated *before*
  // committing to m_model, the same way loadRom() validates before
  // committing to m_romBytes (see its own comment) - a rejected mode
  // change (no ROM loaded yet, or an incompatible Mode::Dmg forced onto a
  // CGB-required cartridge) leaves m_model exactly as it was, which is
  // what keeps reset()'s own "always succeeds" guarantee above true for a
  // caller that falls back to a plain reset() instead.
  [[nodiscard]] std::expected<void, std::string> setMode(Mode mode);

  // The console model this instance currently emulates - the setting
  // passed to the constructor or the last setMode() call, not the
  // per-cartridge resolved HardwareMode (Mode::Auto included: this stays
  // Auto even once a real cartridge has resolved it to DMG or CGB
  // internally). Lets a caller read this back directly instead of having
  // to separately track its own copy of whatever Mode it last requested.
  [[nodiscard]] Mode getMode() const { return m_model; }

  std::expected<EmulationFrame, std::string> runNextFrame();
  void setButtonState(Button button, bool pressed);

  // Captures Cpu/Mmu/Ppu/Apu state (everything except the cartridge ROM
  // and boot ROM data - see Mmu::serialize()'s own comment) into a
  // self-contained buffer a frontend can write to disk/localStorage as-is
  // and hand back to loadState() later, on either this build or another
  // (native <-> Emscripten/wasm) - see SaveStateWriter/SaveStateReader's
  // own comments on why the encoding is portable across both.
  [[nodiscard]] std::vector<std::uint8_t> saveState() const;

  // Restores state written by saveState() - callers are expected to have
  // already loadRom()'d the same cartridge this save was made against
  // (see Mmu::serialize()'s comment on why the ROM itself isn't part of
  // the save file). Rejects (without touching any component's own state)
  // a buffer that doesn't start with the expected magic tag or whose
  // format version doesn't exactly match SAVE_STATE_VERSION - no
  // partial-load or cross-version migration attempt, the same
  // fail-closed approach loadRom() takes for its own untrusted input.
  [[nodiscard]] std::expected<void, std::string> loadState(
    std::span<const std::uint8_t> data);

private:
  // The actual Cpu/Mmu/Ppu/Apu payload, factored out of saveState()/
  // loadState() so loadState() can also use it to snapshot the current
  // state before attempting to overwrite it, and restore that snapshot if
  // a truncated/corrupt body throws partway through - deserializeComponents
  // mutates the four components directly (in the same fixed order
  // serializeComponents wrote them), it isn't itself responsible for the
  // magic/version header both callers already handle around it.
  void serializeComponents(SaveStateWriter& writer) const;
  void deserializeComponents(SaveStateReader& reader);

  // Re-runs the actual ROM/boot-ROM/hardware-mode setup reset() needs,
  // operating on whatever's currently in m_romBytes - the authoritative
  // check on whether m_model and m_romBytes are compatible (a too-small/
  // malformed ROM, or an incompatible Mode::Dmg forced onto a CGB-required
  // cartridge), which loadRom()/setMode() also each validate up front
  // before committing their own piece of state - see reset()'s own
  // comment on why that's what makes it safe for reset() to no longer
  // propagate this failure at all.
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
