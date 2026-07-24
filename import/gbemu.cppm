export module gbemu;

import std;

export import :cpu;
export import :mmu;

export namespace gbemu {

class GameBoy
{
public:
  GameBoy() = default;

  [[nodiscard]] std::expected<void, RomLoadError> loadRom(
    std::span<const std::uint8_t> rom);

private:
  Mmu m_mmu;
  Cpu m_cpu;
};

}
