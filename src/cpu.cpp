module gbemu;

namespace gbemu {

constexpr std::array<Cpu::Instruction, 256> Cpu::INSTRUCTIONS = [] {
  std::array<Instruction, 256> result{};
  result.at(0x00) = { &Cpu::nop };
  result.at(0xC3) = { &Cpu::jpa16 };   // NOLINT(readability-magic-numbers)
  result.at(0xC0) = { &Cpu::retnz };   // NOLINT(readability-magic-numbers)
  result.at(0x21) = { &Cpu::ldhln16 }; // NOLINT(readability-magic-numbers)
  result.at(0x47) = { &Cpu::ldba }; // NOLINT(readability-magic-numbers)
  return result;
}();

std::size_t
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Cpu::nop() // must stay non-static: &Cpu::nop has to match InstructionFun's
           // (Cpu::*)() type, shared by every other (stateful) opcode
{
  m_PC += 1;
  return 1;
}

std::size_t
Cpu::jpa16()
{
  m_PC = m_mmu.get().readWord(m_PC + 1);
  return 4;
}

std::size_t
Cpu::retnz()
{
  if ((m_AF & static_cast<std::uint16_t>(Flag::Zero)) == 0) {
    m_PC = m_mmu.get().readWord(m_SP);
    m_SP += 2;
    return 5; // NOLINT(readability-magic-numbers)
  }
  m_PC += 1;
  return 2;
}

std::size_t
Cpu::ldhln16()
{
  m_HL = m_mmu.get().readWord(m_PC + 1);
  m_PC += 3;
  return 3;
}

std::size_t Cpu::ldba()
{
  m_BC = m_AF;
  m_PC += 1;
  return 1;
}

std::expected<void, std::string>
Cpu::runNextInstruction()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto& instruction = INSTRUCTIONS.at(opcode);
  if (instruction.fun == nullptr) {
    return std::unexpected(
      std::format("Unimplemented opcode: {:#04x}", opcode));
  }

  (this->*instruction.fun)();
  return {};
}

};