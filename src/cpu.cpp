module gbemu;

namespace gbemu {

constexpr std::array<Cpu::Instruction, 256> Cpu::INSTRUCTIONS = [] {
  std::array<Instruction, 256> result{};
  result.at(0x00) = { &Cpu::nop, 1 };
  result.at(0xC3) = { &Cpu::jpa16, 3 }; // NOLINT(readability-magic-numbers)
  return result;
}();

std::expected<std::size_t, std::string>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Cpu::nop() // must stay non-static: &Cpu::nop has to match InstructionFun's
           // (Cpu::*)() type, shared by every other (stateful) opcode
{
  return 1;
}

std::expected<std::size_t, std::string>
Cpu::jpa16()
{
  const auto lowByte = m_mmu.get().readByte(m_PC + 1);
  const auto highByte = m_mmu.get().readByte(m_PC + 2);
  m_PC = static_cast<std::uint16_t>((static_cast<unsigned>(highByte) << 8U) |
                                    static_cast<unsigned>(lowByte));
  return 4;
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

  const auto result = (this->*instruction.fun)();
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }
  m_PC += instruction.length;
  return {};
}

};