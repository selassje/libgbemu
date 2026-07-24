export module gbemu:cpu;

import std;
import :mmu;

namespace gbemu {

class Cpu // NOLINT(misc-use-internal-linkage)
{
public:
  explicit Cpu(Mmu& mmu)
    : m_mmu(mmu)
  {
  }

  std::expected<void, std::string> runNextInstruction();

private:
  enum class Flag : std::uint8_t
  {
    Zero = 0x80,
    Subtract = 0x40,
    HalfCarry = 0x20,
    Carry = 0x10,
  };

  std::uint16_t m_AF{ 0 };
  std::uint16_t m_BC{ 0 };
  std::uint16_t m_DE{ 0 };
  std::uint16_t m_HL{ 0 };
  std::uint16_t m_SP{ 0 };
  std::uint16_t m_PC{ 0x100 };

  std::reference_wrapper<Mmu> m_mmu;

  using InstructionFun = std::expected<std::size_t, std::string> (Cpu::*)();

  struct Instruction
  {
    InstructionFun fun = nullptr;
  };

  std::expected<std::size_t, std::string> nop();
  std::expected<std::size_t, std::string> jpa16();
  std::expected<std::size_t, std::string> retnz();
  std::expected<std::size_t, std::string> ldhln16();

  static const std::array<Instruction, 256> INSTRUCTIONS;
};

}
