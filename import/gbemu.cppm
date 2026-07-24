export module gbemu;

import std;

export import :cpu;
export import :mmu;

export namespace gbemu {

class GameBoy
{
public:
  GameBoy() = default;

  [[nodiscard]] std::expected<void, std::string> loadRom(
    std::span<const std::uint8_t> rom);

  std::expected<void, std::string> runNextInstruction();

private:
  Mmu m_mmu;
  Cpu m_cpu;
};

}
