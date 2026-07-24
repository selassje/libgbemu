module gbemu;

namespace gbemu {

std::expected<std::size_t, std::string>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Cpu::nop() // must stay non-static: &Cpu::nop has to match InstructionFun's
           // (Cpu::*)() type, shared by every other (stateful) opcode
{
  return 1;
}

std::expected<void, std::string>
Cpu::runNextInstruction()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto& instruction = INSTRUCTIONS.at(opcode);
  if (instruction.fun == nullptr) {
    return std::unexpected("Unimplemented opcode: " + std::to_string(opcode));
  }

  const auto result = (this->*instruction.fun)();
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  m_PC += instruction.length;
  return {};
}

};