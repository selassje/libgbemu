export module gbemu;

import std;

export import :apu;
export import :cpu;
export import :mmu;
export import :ppu;
export import :boot_rom;
export import :regs;
export import :serialization;
export import :hardware_mode;
export import :mappers;
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
  std::mdspan<const float, std::extents<std::size_t, std::dynamic_extent, 2>>
    audio;
};

enum class Mode : std::uint8_t
{
  // Boots as DMG for a cartridge that doesn't declare CGB support/
  // requirement (header byte 0x0143), or as CGB for one that does.
  Auto,
  // Always boots as DMG - rejected for a cartridge whose header declares
  // CGB as *required* (0x0143 bits 7+6 both set, i.e. 0xC0-0xFF) rather
  // than merely supported, since no real DMG console could run that
  // cartridge correctly either.
  Dmg,
  // Always boots as CGB hardware. Lands in CGB compatibility mode for a
  // DMG-only cartridge, or native CGB mode for a cartridge that itself
  // declares CGB support.
  Cgb,
};

class GameBoy
{
public:
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

  void reset();

  [[nodiscard]] std::expected<void, std::string> setMode(Mode mode);

  [[nodiscard]] Mode getMode() const { return m_model; }

  std::expected<EmulationFrame, std::string> runNextFrame();
  void setButtonState(Button button, bool pressed);

  [[nodiscard]] std::vector<std::uint8_t> saveState() const;

  [[nodiscard]] std::expected<void, std::string> loadState(
    std::span<const std::uint8_t> data);

private:
  void serializeComponents(SaveStateWriter& writer) const;
  void deserializeComponents(SaveStateReader& reader);

  [[nodiscard]] std::expected<void, std::string> initializeFromRom();

  Mode m_model;
  Apu m_apu;
  Mmu m_mmu;
  Ppu m_ppu;
  Cpu m_cpu;
  HardwareMode m_hardwareMode{ HardwareMode::Dmg };
  std::vector<std::uint8_t> m_romBytes;
};

}
