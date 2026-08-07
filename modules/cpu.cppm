export module gbemu:cpu;

import std;
import :mmu;
import :ppu;
import :apu;
import :serialization;

namespace gbemu {

class Cpu // NOLINT(misc-use-internal-linkage)
{
public:
  Cpu(Mmu& mmu, Ppu& ppu, Apu& apu)
    : m_mmu(mmu)
    , m_ppu(ppu)
    , m_apu(apu)
  {
  }

  // True hardware power-on/reset state (PC=0, SP=0xFFFF, all other
  // registers/flags zero) - the boot ROM (see :boot_rom) is what brings the
  // CPU to a real post-boot state by actually executing, the same as on
  // real hardware; this is not a "seed the post-boot values" shortcut.
  void reset();

  std::expected<std::size_t, std::string> runNextInstruction();
  [[nodiscard]] std::size_t baseTCycles() const { return m_baseTCycles; }

  // Save-state support (see GameBoy::saveState()/loadState()) - every data
  // member below except the Mmu/Ppu/Apu references (owned/reloaded
  // separately) and the static instruction table (fixed, not runtime
  // state).
  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

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
  std::uint16_t m_SP{ 0xFFFF };
  std::uint16_t m_PC{ 0 };
  bool m_ime{ false };
  bool m_halted{ false };
  bool m_haltBugPending{ false };
  std::uint8_t m_currentOpcode{ 0 };
  std::uint8_t m_imeEnableDelay{ 0 };

  std::size_t m_mcycles{ 0 };
  std::size_t m_lastTimerMCycles{ 0 };
  // How many CPU T-cycles have already been synchronized. MMU's CPU-clocked
  // state advances on every one; PPU/APU advance on every cycle at normal
  // speed and every other cycle at double speed.
  std::size_t m_syncedTCycles{ 0 };
  // Base-speed clock count used by GameBoy::runNextFrame(), so a frame keeps
  // the same real duration even when speed changes partway through it.
  std::size_t m_baseTCycles{ 0 };
  bool m_doubleSpeedPhase{ false };

  std::reference_wrapper<Mmu> m_mmu;
  std::reference_wrapper<Ppu> m_ppu;
  std::reference_wrapper<Apu> m_apu;

  using InstructionFun = std::size_t (Cpu::*)();

  struct Instruction
  {
    InstructionFun fun = nullptr;
  };

  [[nodiscard]] std::uint8_t getR8(std::uint8_t code) const;
  void setR8(std::uint8_t code, std::uint8_t value);
  void applyAluOp(std::uint8_t op, std::uint8_t operand);

  void handleInterrupts();
  // Catches Ppu/Mmu/Apu up to currentTCycles one T-cycle at a time (so a
  // memory access made partway through an instruction sees hardware state
  // as of its own T-cycle, not just whatever was left over from the
  // *previous* instruction), then runs the timer's own DIV/TIMA catch-up
  // math up to timerMCycles - a separate parameter (not just
  // currentTCycles/4) specifically so a caller needing sub-M-cycle
  // precision for the *peripheral* observation point (see ldha8(), the
  // only current user) doesn't also drag the timer's own, genuinely
  // M-cycle-granular catch-up point backward with it. Called at every
  // point in an instruction handler that performs a memory access - the
  // same checkpoints this used to only serve for the timer, before it
  // needed T-cycle precision - plus once more at the end of
  // runNextInstruction() with the instruction's final cycle totals, to
  // flush any of its own trailing cycles a mid-instruction checkpoint
  // didn't already cover.
  void advanceHardware(std::size_t currentTCycles, std::size_t timerMCycles);
  // Convenience overload for every call site *except* ldha8()'s Wave RAM
  // read: currentTCycles is always an exact M-cycle multiple for these, so
  // currentTCycles/4 is genuinely the same M-cycle the timer should catch
  // up to.
  void advanceHardware(std::size_t currentTCycles);
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
