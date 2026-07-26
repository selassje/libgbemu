export module gbemu:cpu;

import std;
import :mmu;
import :ppu;

namespace gbemu {

class Cpu // NOLINT(misc-use-internal-linkage)
{
public:
  Cpu(Mmu& mmu, Ppu& ppu)
    : m_mmu(mmu)
    , m_ppu(ppu)
  {
  }

  // True hardware power-on/reset state (PC=0, SP=0xFFFF, all other
  // registers/flags zero) - the boot ROM (see :boot_rom) is what brings the
  // CPU to a real post-boot state by actually executing, the same as on
  // real hardware; this is not a "seed the post-boot values" shortcut.
  void reset();

  std::expected<std::size_t, std::string> runNextInstruction();

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
  std::uint16_t m_SP{ 0xFFFF }; // NOLINT(readability-magic-numbers)
  std::uint16_t m_PC{ 0 };
  bool m_ime{ false };
  bool m_halted{ false };
  bool m_haltBugPending{ false };
  std::uint8_t m_currentOpcode{ 0 };
  std::uint8_t m_imeEnableDelay{ 0 };

  std::size_t m_mcycles{ 0 };
  std::size_t m_lastTimerMCycles{ 0 };

  std::reference_wrapper<Mmu> m_mmu;
  std::reference_wrapper<Ppu> m_ppu;

  using InstructionFun = std::size_t (Cpu::*)();

  struct Instruction
  {
    InstructionFun fun = nullptr;
  };

  [[nodiscard]] std::uint8_t getR8(std::uint8_t code) const;
  void setR8(std::uint8_t code, std::uint8_t value);
  void applyAluOp(std::uint8_t op, std::uint8_t operand);

  void handleInterrupts();
  void handleTimer(std::size_t currentMCycles);
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
