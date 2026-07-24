module gbemu;

namespace gbemu {

constexpr std::array<Cpu::Instruction, 256> Cpu::INSTRUCTIONS = [] {
  std::array<Instruction, 256> result{};
  result.at(0x00) = { &Cpu::nop };
  result.at(0xC3) = { &Cpu::jpa16 }; // NOLINT(readability-magic-numbers)
  result.at(0xC0) = { &Cpu::retnz }; // NOLINT(readability-magic-numbers)
  constexpr std::size_t ldRRd16First = 0x01;
  constexpr std::size_t ldRRd16Last = 0x31;
  constexpr std::size_t ldRRd16Step = 0x10;
  for (std::size_t opcode = ldRRd16First; opcode <= ldRRd16Last;
       opcode += ldRRd16Step) {
    result.at(opcode) = { &Cpu::ldRRd16 };
  }
  constexpr std::size_t ldRRFirst = 0x40;
  constexpr std::size_t ldRRLast = 0x7F;
  constexpr std::size_t halt = 0x76;
  for (std::size_t opcode = ldRRFirst; opcode <= ldRRLast; ++opcode) {
    if (opcode != halt) { // HALT, not LD (HL),(HL)
      result.at(opcode) = { &Cpu::ldRR };
    }
  }
  constexpr std::size_t ldRd8First = 0x06;
  constexpr std::size_t ldRd8Last = 0x3E;
  constexpr std::size_t ldRd8Step = 0x08;
  for (std::size_t opcode = ldRd8First; opcode <= ldRd8Last;
       opcode += ldRd8Step) {
    result.at(opcode) = { &Cpu::ldRd8 };
  }
  result.at(0x22) = { &Cpu::ldhlia };  // NOLINT(readability-magic-numbers)
  result.at(0x2A) = { &Cpu::ldhlia };  // NOLINT(readability-magic-numbers)
  result.at(0x32) = { &Cpu::ldhlia };  // NOLINT(readability-magic-numbers)
  result.at(0x3A) = { &Cpu::ldhlia };  // NOLINT(readability-magic-numbers)
  result.at(0x02) = { &Cpu::ldbcdea }; // NOLINT(readability-magic-numbers)
  result.at(0x12) = { &Cpu::ldbcdea }; // NOLINT(readability-magic-numbers)
  result.at(0x0A) = { &Cpu::ldbcdea }; // NOLINT(readability-magic-numbers)
  result.at(0x1A) = { &Cpu::ldbcdea }; // NOLINT(readability-magic-numbers)
  constexpr std::size_t incr8First = 0x04;
  constexpr std::size_t incr8Last = 0x3C;
  constexpr std::size_t incr8Step = 0x08;
  for (std::size_t opcode = incr8First; opcode <= incr8Last;
       opcode += incr8Step) {
    result.at(opcode) = { &Cpu::incr8 };
  }
  result.at(0x18) = { &Cpu::jrcc }; // NOLINT(readability-magic-numbers)
  result.at(0x20) = { &Cpu::jrcc }; // NOLINT(readability-magic-numbers)
  result.at(0x28) = { &Cpu::jrcc }; // NOLINT(readability-magic-numbers)
  result.at(0x30) = { &Cpu::jrcc }; // NOLINT(readability-magic-numbers)
  result.at(0x38) = { &Cpu::jrcc }; // NOLINT(readability-magic-numbers)
  return result;
}();

namespace {
constexpr std::uint8_t REG_B = 0x0;
constexpr std::uint8_t REG_C = 0x1;
constexpr std::uint8_t REG_D = 0x2;
constexpr std::uint8_t REG_E = 0x3;
constexpr std::uint8_t REG_H = 0x4;
constexpr std::uint8_t REG_L = 0x5;
constexpr std::uint8_t REG_HL_INDIRECT = 0x6;
constexpr std::uint8_t REG_A = 0x7;

constexpr std::uint16_t LOW_BYTE_MASK = 0x00FF;
constexpr std::uint16_t HIGH_BYTE_MASK = 0xFF00;
constexpr std::uint8_t NIBBLE_MASK = 0x0F;
}

std::uint8_t
Cpu::getR8(std::uint8_t code) const
{
  switch (code) {
    case REG_B:
      return static_cast<std::uint8_t>(m_BC >> 8U);
    case REG_C:
      return static_cast<std::uint8_t>(m_BC);
    case REG_D:
      return static_cast<std::uint8_t>(m_DE >> 8U);
    case REG_E:
      return static_cast<std::uint8_t>(m_DE);
    case REG_H:
      return static_cast<std::uint8_t>(m_HL >> 8U);
    case REG_L:
      return static_cast<std::uint8_t>(m_HL);
    case REG_A:
      return static_cast<std::uint8_t>(m_AF >> 8U);
    default:
      return 0; // REG_HL_INDIRECT, handled separately by the caller
  }
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Cpu::setR8(std::uint8_t code, std::uint8_t value)
{
  const auto highByte = static_cast<unsigned>(value) << 8U;
  switch (code) {
    case REG_B:
      m_BC = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_BC) & LOW_BYTE_MASK) | highByte);
      break;
    case REG_C:
      m_BC = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_BC) & HIGH_BYTE_MASK) |
        static_cast<unsigned>(value));
      break;
    case REG_D:
      m_DE = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_DE) & LOW_BYTE_MASK) | highByte);
      break;
    case REG_E:
      m_DE = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_DE) & HIGH_BYTE_MASK) |
        static_cast<unsigned>(value));
      break;
    case REG_H:
      m_HL = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_HL) & LOW_BYTE_MASK) | highByte);
      break;
    case REG_L:
      m_HL = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_HL) & HIGH_BYTE_MASK) |
        static_cast<unsigned>(value));
      break;
    case REG_A:
      m_AF = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_AF) & LOW_BYTE_MASK) | highByte);
      break;
    default:
      break; // REG_HL_INDIRECT, handled separately by the caller
  }
}

std::size_t
Cpu::nop()
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
Cpu::ldRRd16()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto r16 =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 4U) & 0x03U);
  const auto value = m_mmu.get().readWord(m_PC + 1);

  switch (r16) {
    case 0x0:
      m_BC = value;
      break;
    case 0x1:
      m_DE = value;
      break;
    case 0x2:
      m_HL = value;
      break;
    default: // 0x3
      m_SP = value;
      break;
  }

  m_PC += 3;
  return 3;
}

std::size_t
Cpu::ldRR()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto dst =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x07U);
  const auto src =
    static_cast<std::uint8_t>(static_cast<unsigned>(opcode) & 0x07U);

  std::size_t cycles = 1;

  std::uint8_t value = 0;
  if (src == REG_HL_INDIRECT) {
    value = m_mmu.get().readByte(m_HL);
    cycles = 2; // NOLINT(readability-magic-numbers)
  } else {
    value = getR8(src);
  }

  if (dst == REG_HL_INDIRECT) {
    m_mmu.get().writeByte(m_HL, value);
    cycles = 2; // NOLINT(readability-magic-numbers)
  } else {
    setR8(dst, value);
  }

  m_PC += 1;
  return cycles;
}

std::size_t
Cpu::ldRd8()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto dst =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x07U);
  const auto value = m_mmu.get().readByte(m_PC + 1);

  std::size_t cycles = 2; // NOLINT(readability-magic-numbers)
  if (dst == REG_HL_INDIRECT) {
    m_mmu.get().writeByte(m_HL, value);
    cycles = 3; // NOLINT(readability-magic-numbers)
  } else {
    setR8(dst, value);
  }

  m_PC += 2;
  return cycles;
}

std::size_t
Cpu::ldhlia()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const bool load = (static_cast<unsigned>(opcode) & 0x08U) != 0;
  const bool decrement = (static_cast<unsigned>(opcode) & 0x10U) != 0;

  if (load) {
    setR8(REG_A, m_mmu.get().readByte(m_HL));
  } else {
    m_mmu.get().writeByte(m_HL, getR8(REG_A));
  }

  m_HL = static_cast<std::uint16_t>(decrement ? m_HL - 1 : m_HL + 1);

  m_PC += 1;
  return 2;
}

std::size_t
Cpu::ldbcdea()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const bool useDE = (static_cast<unsigned>(opcode) & 0x10U) != 0;
  const bool load = (static_cast<unsigned>(opcode) & 0x08U) != 0;
  const auto address = useDE ? m_DE : m_BC;

  if (load) {
    setR8(REG_A, m_mmu.get().readByte(address));
  } else {
    m_mmu.get().writeByte(address, getR8(REG_A));
  }

  m_PC += 1;
  return 2;
}

std::size_t
Cpu::incr8()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto reg =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x07U);

  std::uint8_t oldValue = 0;
  std::size_t cycles = 1;
  if (reg == REG_HL_INDIRECT) {
    oldValue = m_mmu.get().readByte(m_HL);
    cycles = 3; // NOLINT(readability-magic-numbers)
  } else {
    oldValue = getR8(reg);
  }

  const auto newValue = static_cast<std::uint8_t>(oldValue + 1);

  if (reg == REG_HL_INDIRECT) {
    m_mmu.get().writeByte(m_HL, newValue);
  } else {
    setR8(reg, newValue);
  }

  auto flags = static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Carry);
  if (newValue == 0) {
    flags |= static_cast<unsigned>(Flag::Zero);
  }
  if ((oldValue & NIBBLE_MASK) == NIBBLE_MASK) {
    flags |= static_cast<unsigned>(Flag::HalfCarry);
  }
  m_AF = static_cast<std::uint16_t>(
    (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);

  m_PC += 1;
  return cycles;
}

std::size_t
Cpu::jrcc()
{
  constexpr std::size_t unconditionalOpcode = 0x18;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto offset = static_cast<std::int8_t>(m_mmu.get().readByte(m_PC + 1));
  m_PC += 2;

  bool takeBranch = true;
  if (opcode != unconditionalOpcode) {
    const auto cc =
      static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x03U);
    const bool zeroSet = (m_AF & static_cast<std::uint16_t>(Flag::Zero)) != 0;
    const bool carrySet = (m_AF & static_cast<std::uint16_t>(Flag::Carry)) != 0;
    switch (cc) {
      case 0x0:
        takeBranch = !zeroSet; // NZ
        break;
      case 0x1:
        takeBranch = zeroSet; // Z
        break;
      case 0x2:
        takeBranch = !carrySet; // NC
        break;
      default:
        takeBranch = carrySet; // C
        break;
    }
  }

  if (takeBranch) {
    m_PC = static_cast<std::uint16_t>(static_cast<int>(m_PC) + offset);
    return 3; // NOLINT(readability-magic-numbers)
  }
  return 2;
}

std::expected<std::size_t, std::string>
Cpu::runNextInstruction()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto& instruction = INSTRUCTIONS.at(opcode);
  if (instruction.fun == nullptr) {
    return std::unexpected(
      std::format("Unimplemented opcode: {:#04x}", opcode));
  }
  return (this->*instruction.fun)();
}

};