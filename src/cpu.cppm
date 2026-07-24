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

  std::expected<std::size_t, std::string> runNextInstruction();

private:
  enum class Flag : std::uint8_t
  {
    Zero = 0x80,
    Subtract = 0x40,
    HalfCarry = 0x20,
    Carry = 0x10,
  };

  // post-boot-ROM DMG/SGB state: A=0x01, F=Z100
  std::uint16_t m_AF{ 0x01B0 }; // NOLINT(readability-magic-numbers)
  std::uint16_t m_BC{ 0 };
  std::uint16_t m_DE{ 0 };
  std::uint16_t m_HL{ 0 };
  std::uint16_t m_SP{ 0 };
  std::uint16_t m_PC{ 0x100 };
  bool m_ime{ false };
  bool m_halted{ false };

  std::size_t m_cycles{ 0 };
  std::size_t m_lastTimerCyclesIncrement{ 0 };

  std::reference_wrapper<Mmu> m_mmu;

  using InstructionFun = std::size_t (Cpu::*)();

  struct Instruction
  {
    InstructionFun fun = nullptr;
  };

  [[nodiscard]] std::uint8_t getR8(std::uint8_t code) const;
  void setR8(std::uint8_t code, std::uint8_t value);
  void applyAluOp(std::uint8_t op, std::uint8_t operand);

  std::size_t handleInterrupts();
  void handleTimer();
  [[nodiscard]] bool interruptRequestPending() const;

  std::size_t nop();
  std::size_t halt();
  std::size_t jpcc();
  std::size_t retcc();
  std::size_t ldRRd16();
  std::size_t ldRR();
  std::size_t ldRd8();
  std::size_t ldhlia();
  std::size_t ldbcdea();
  std::size_t incr8();
  std::size_t decr8();
  std::size_t jrcc();
  std::size_t di();
  std::size_t ei();
  std::size_t stop();
  std::size_t reti();
  std::size_t rst();
  std::size_t ldaa16();
  std::size_t ldha8();
  std::size_t ldhca();
  std::size_t callcc();
  std::size_t pushr16();
  std::size_t popr16();
  std::size_t incdecr16();
  std::size_t aluR8();
  std::size_t aluD8();
  std::size_t cbPrefixed();
  std::size_t rotateA();
  std::size_t addhlr16();
  std::size_t jphl();
  std::size_t ldA16Sp();
  std::size_t addSpHlE8();
  std::size_t ldSpHl();
  std::size_t daaCplScfCcf();

  static const std::array<Instruction, 256> INSTRUCTIONS;
};

}
