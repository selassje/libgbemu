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

  using InstructionFun = std::size_t (Cpu::*)();

  struct Instruction
  {
    InstructionFun fun = nullptr;
  };

  [[nodiscard]] std::uint8_t getR8(std::uint8_t code) const;
  void setR8(std::uint8_t code, std::uint8_t value);

  std::size_t nop();
  std::size_t jpa16();
  std::size_t retnz();
  std::size_t ldRRd16();
  std::size_t ldRR();
  std::size_t ldRd8();
  std::size_t ldhlia();
  std::size_t ldbcdea();
  std::size_t incr8();
  std::size_t jrcc();

  static const std::array<Instruction, 256> INSTRUCTIONS;
};

}
