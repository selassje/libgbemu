module gbemu;

namespace gbemu {

constexpr std::array<Cpu::Instruction, 256> Cpu::INSTRUCTIONS = [] {
  std::array<Instruction, 256> result{};
  result.at(0x00) = { &Cpu::nop };
  result.at(0xC2) = { &Cpu::jpcc };  // NOLINT(readability-magic-numbers)
  result.at(0xC3) = { &Cpu::jpcc };  // NOLINT(readability-magic-numbers)
  result.at(0xCA) = { &Cpu::jpcc };  // NOLINT(readability-magic-numbers)
  result.at(0xD2) = { &Cpu::jpcc };  // NOLINT(readability-magic-numbers)
  result.at(0xDA) = { &Cpu::jpcc };  // NOLINT(readability-magic-numbers)
  result.at(0xE9) = { &Cpu::jphl };  // NOLINT(readability-magic-numbers)
  result.at(0xC0) = { &Cpu::retcc }; // NOLINT(readability-magic-numbers)
  result.at(0xC8) = { &Cpu::retcc }; // NOLINT(readability-magic-numbers)
  result.at(0xD0) = { &Cpu::retcc }; // NOLINT(readability-magic-numbers)
  result.at(0xD8) = { &Cpu::retcc }; // NOLINT(readability-magic-numbers)
  result.at(0xC9) = { &Cpu::retcc }; // NOLINT(readability-magic-numbers)
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
  constexpr std::size_t decr8First = 0x05;
  constexpr std::size_t decr8Last = 0x3D;
  constexpr std::size_t decr8Step = 0x08;
  for (std::size_t opcode = decr8First; opcode <= decr8Last;
       opcode += decr8Step) {
    result.at(opcode) = { &Cpu::decr8 };
  }
  result.at(0x18) = { &Cpu::jrcc };         // NOLINT(readability-magic-numbers)
  result.at(0x20) = { &Cpu::jrcc };         // NOLINT(readability-magic-numbers)
  result.at(0x28) = { &Cpu::jrcc };         // NOLINT(readability-magic-numbers)
  result.at(0x30) = { &Cpu::jrcc };         // NOLINT(readability-magic-numbers)
  result.at(0x38) = { &Cpu::jrcc };         // NOLINT(readability-magic-numbers)
  result.at(0x10) = { &Cpu::stop };         // NOLINT(readability-magic-numbers)
  result.at(0xF3) = { &Cpu::di };           // NOLINT(readability-magic-numbers)
  result.at(0xFB) = { &Cpu::ei };           // NOLINT(readability-magic-numbers)
  result.at(0xEA) = { &Cpu::ldaa16 };       // NOLINT(readability-magic-numbers)
  result.at(0xFA) = { &Cpu::ldaa16 };       // NOLINT(readability-magic-numbers)
  result.at(0xE0) = { &Cpu::ldha8 };        // NOLINT(readability-magic-numbers)
  result.at(0xF0) = { &Cpu::ldha8 };        // NOLINT(readability-magic-numbers)
  result.at(0xCB) = { &Cpu::cbPrefixed };   // NOLINT(readability-magic-numbers)
  result.at(0x07) = { &Cpu::rotateA };      // NOLINT(readability-magic-numbers)
  result.at(0x0F) = { &Cpu::rotateA };      // NOLINT(readability-magic-numbers)
  result.at(0x17) = { &Cpu::rotateA };      // NOLINT(readability-magic-numbers)
  result.at(0x1F) = { &Cpu::rotateA };      // NOLINT(readability-magic-numbers)
  result.at(0x27) = { &Cpu::daaCplScfCcf }; // NOLINT(readability-magic-numbers)
  result.at(0x2F) = { &Cpu::daaCplScfCcf }; // NOLINT(readability-magic-numbers)
  result.at(0x37) = { &Cpu::daaCplScfCcf }; // NOLINT(readability-magic-numbers)
  result.at(0x3F) = { &Cpu::daaCplScfCcf }; // NOLINT(readability-magic-numbers)
  result.at(0xC4) = { &Cpu::callcc };       // NOLINT(readability-magic-numbers)
  result.at(0xCC) = { &Cpu::callcc };       // NOLINT(readability-magic-numbers)
  result.at(0xD4) = { &Cpu::callcc };       // NOLINT(readability-magic-numbers)
  result.at(0xDC) = { &Cpu::callcc };       // NOLINT(readability-magic-numbers)
  result.at(0xCD) = { &Cpu::callcc };       // NOLINT(readability-magic-numbers)
  constexpr std::size_t pushFirst = 0xC5;
  constexpr std::size_t pushLast = 0xF5;
  constexpr std::size_t popFirst = 0xC1;
  constexpr std::size_t popLast = 0xF1;
  constexpr std::size_t pushPopStep = 0x10;
  for (std::size_t opcode = pushFirst; opcode <= pushLast;
       opcode += pushPopStep) {
    result.at(opcode) = { &Cpu::pushr16 };
  }
  for (std::size_t opcode = popFirst; opcode <= popLast;
       opcode += pushPopStep) {
    result.at(opcode) = { &Cpu::popr16 };
  }
  constexpr std::size_t incr16First = 0x03;
  constexpr std::size_t incr16Last = 0x33;
  constexpr std::size_t decr16First = 0x0B;
  constexpr std::size_t decr16Last = 0x3B;
  constexpr std::size_t incdecr16Step = 0x10;
  for (std::size_t opcode = incr16First; opcode <= incr16Last;
       opcode += incdecr16Step) {
    result.at(opcode) = { &Cpu::incdecr16 };
  }
  for (std::size_t opcode = decr16First; opcode <= decr16Last;
       opcode += incdecr16Step) {
    result.at(opcode) = { &Cpu::incdecr16 };
  }
  constexpr std::size_t addhlr16First = 0x09;
  constexpr std::size_t addhlr16Last = 0x39;
  constexpr std::size_t addhlr16Step = 0x10;
  for (std::size_t opcode = addhlr16First; opcode <= addhlr16Last;
       opcode += addhlr16Step) {
    result.at(opcode) = { &Cpu::addhlr16 };
  }
  result.at(0x08) = { &Cpu::ldA16Sp };   // NOLINT(readability-magic-numbers)
  result.at(0xE8) = { &Cpu::addSpHlE8 }; // NOLINT(readability-magic-numbers)
  result.at(0xF8) = { &Cpu::addSpHlE8 }; // NOLINT(readability-magic-numbers)
  result.at(0xF9) = { &Cpu::ldSpHl };    // NOLINT(readability-magic-numbers)
  constexpr std::size_t aluR8First = 0x80;
  constexpr std::size_t aluR8Last = 0xBF;
  for (std::size_t opcode = aluR8First; opcode <= aluR8Last; ++opcode) {
    result.at(opcode) = { &Cpu::aluR8 };
  }
  constexpr std::size_t aluD8First = 0xC6;
  constexpr std::size_t aluD8Last = 0xFE;
  constexpr std::size_t aluD8Step = 0x08;
  for (std::size_t opcode = aluD8First; opcode <= aluD8Last;
       opcode += aluD8Step) {
    result.at(opcode) = { &Cpu::aluD8 };
  }
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
constexpr std::uint16_t IO_REGISTERS_BASE = 0xFF00;
constexpr std::uint16_t FLAGS_UNUSED_BITS_MASK = 0xFFF0;
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
Cpu::jpcc()
{
  constexpr std::size_t unconditionalOpcode = 0xC3;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto target = m_mmu.get().readWord(m_PC + 1);

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
    m_PC = target;
    return 4; // NOLINT(readability-magic-numbers)
  }
  m_PC += 3;
  return 3; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::retcc()
{
  constexpr std::size_t unconditionalOpcode = 0xC9;

  const auto opcode = m_mmu.get().readByte(m_PC);

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
    m_PC = m_mmu.get().readWord(m_SP);
    m_SP += 2;
    return opcode == unconditionalOpcode
             ? 4
             : 5; // NOLINT(readability-magic-numbers)
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
Cpu::decr8()
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

  const auto newValue = static_cast<std::uint8_t>(oldValue - 1);

  if (reg == REG_HL_INDIRECT) {
    m_mmu.get().writeByte(m_HL, newValue);
  } else {
    setR8(reg, newValue);
  }

  auto flags = static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Carry);
  flags |= static_cast<unsigned>(Flag::Subtract);
  if (newValue == 0) {
    flags |= static_cast<unsigned>(Flag::Zero);
  }
  if ((oldValue & NIBBLE_MASK) == 0) {
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

std::size_t
Cpu::di()
{
  m_ime = false;
  m_PC += 1;
  return 1;
}

std::size_t
Cpu::ei()
{
  m_ime = true;
  m_PC += 1;
  return 1;
}

std::size_t
Cpu::stop()
{
  // STOP is a 2-byte instruction (the second byte is a mandatory padding
  // byte); real low-power/speed-switch behavior isn't implemented yet since
  // nothing can wake the CPU from it, so this just skips both bytes.
  m_PC += 2;
  return 1;
}

std::size_t
Cpu::ldaa16()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const bool load = (static_cast<unsigned>(opcode) & 0x10U) != 0;
  const auto address = m_mmu.get().readWord(m_PC + 1);

  if (load) {
    setR8(REG_A, m_mmu.get().readByte(address));
  } else {
    m_mmu.get().writeByte(address, getR8(REG_A));
  }

  m_PC += 3;
  return 4; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::ldha8()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const bool load = (static_cast<unsigned>(opcode) & 0x10U) != 0;
  const auto offset = m_mmu.get().readByte(m_PC + 1);
  const auto address = static_cast<std::uint16_t>(IO_REGISTERS_BASE + offset);

  if (load) {
    setR8(REG_A, m_mmu.get().readByte(address));
  } else {
    m_mmu.get().writeByte(address, getR8(REG_A));
  }

  m_PC += 2;
  return 3; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::callcc()
{
  constexpr std::size_t unconditionalOpcode = 0xCD;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto target = m_mmu.get().readWord(m_PC + 1);
  const auto returnAddress = static_cast<std::uint16_t>(m_PC + 3);

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
    m_SP -= 2;
    m_mmu.get().writeWord(m_SP, returnAddress);
    m_PC = target;
    return 6; // NOLINT(readability-magic-numbers)
  }
  m_PC = returnAddress;
  return 3; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::pushr16()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto r16 =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 4U) & 0x03U);

  std::uint16_t value = 0;
  switch (r16) {
    case 0x0:
      value = m_BC;
      break;
    case 0x1:
      value = m_DE;
      break;
    case 0x2:
      value = m_HL;
      break;
    default: // 0x3
      value = m_AF;
      break;
  }

  m_SP -= 2;
  m_mmu.get().writeWord(m_SP, value);

  m_PC += 1;
  return 4; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::popr16()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto r16 =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 4U) & 0x03U);

  const auto value = m_mmu.get().readWord(m_SP);
  m_SP += 2;

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
    default: // 0x3, AF: the low nibble of F is always 0 on real hardware
      m_AF = static_cast<std::uint16_t>(static_cast<unsigned>(value) &
                                        FLAGS_UNUSED_BITS_MASK);
      break;
  }

  m_PC += 1;
  return 3; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::incdecr16()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto r16 =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 4U) & 0x03U);
  const bool decrement = (static_cast<unsigned>(opcode) & 0x08U) != 0;

  switch (r16) {
    case 0x0:
      m_BC = static_cast<std::uint16_t>(decrement ? m_BC - 1 : m_BC + 1);
      break;
    case 0x1:
      m_DE = static_cast<std::uint16_t>(decrement ? m_DE - 1 : m_DE + 1);
      break;
    case 0x2:
      m_HL = static_cast<std::uint16_t>(decrement ? m_HL - 1 : m_HL + 1);
      break;
    default: // 0x3
      m_SP = static_cast<std::uint16_t>(decrement ? m_SP - 1 : m_SP + 1);
      break;
  }

  m_PC += 1;
  return 2;
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Cpu::applyAluOp(std::uint8_t op, std::uint8_t operand)
{
  constexpr unsigned byteOverflow = 0x100;
  constexpr std::uint8_t opAdd = 0x0;
  constexpr std::uint8_t opAdc = 0x1;
  constexpr std::uint8_t opSub = 0x2;
  constexpr std::uint8_t opSbc = 0x3;
  constexpr std::uint8_t opAnd = 0x4;
  constexpr std::uint8_t opXor = 0x5;
  constexpr std::uint8_t opOr = 0x6;

  const auto a = static_cast<unsigned>(getR8(REG_A));
  const auto operandValue = static_cast<unsigned>(operand);
  const bool carryIn = (m_AF & static_cast<std::uint16_t>(Flag::Carry)) != 0;
  const unsigned carryInValue = carryIn ? 1 : 0;

  unsigned result = 0;
  unsigned flags = 0;
  bool storeResult = true;

  switch (op) {
    case opAdd:
      result = a + operandValue;
      if (((a & NIBBLE_MASK) + (operandValue & NIBBLE_MASK)) > NIBBLE_MASK) {
        flags |= static_cast<unsigned>(Flag::HalfCarry);
      }
      if (result >= byteOverflow) {
        flags |= static_cast<unsigned>(Flag::Carry);
      }
      break;
    case opAdc:
      result = a + operandValue + carryInValue;
      if (((a & NIBBLE_MASK) + (operandValue & NIBBLE_MASK) + carryInValue) >
          NIBBLE_MASK) {
        flags |= static_cast<unsigned>(Flag::HalfCarry);
      }
      if (result >= byteOverflow) {
        flags |= static_cast<unsigned>(Flag::Carry);
      }
      break;
    case opSub:
      result = a - operandValue;
      flags |= static_cast<unsigned>(Flag::Subtract);
      if ((a & NIBBLE_MASK) < (operandValue & NIBBLE_MASK)) {
        flags |= static_cast<unsigned>(Flag::HalfCarry);
      }
      if (a < operandValue) {
        flags |= static_cast<unsigned>(Flag::Carry);
      }
      break;
    case opSbc: {
      const unsigned subtrahend = operandValue + carryInValue;
      result = a - subtrahend;
      flags |= static_cast<unsigned>(Flag::Subtract);
      if ((a & NIBBLE_MASK) < ((operandValue & NIBBLE_MASK) + carryInValue)) {
        flags |= static_cast<unsigned>(Flag::HalfCarry);
      }
      if (a < subtrahend) {
        flags |= static_cast<unsigned>(Flag::Carry);
      }
      break;
    }
    case opAnd:
      result = a & operandValue;
      flags |= static_cast<unsigned>(Flag::HalfCarry);
      break;
    case opXor:
      result = a ^ operandValue;
      break;
    case opOr:
      result = a | operandValue;
      break;
    default: // CP: same as SUB, but the result is discarded
      result = a - operandValue;
      flags |= static_cast<unsigned>(Flag::Subtract);
      if ((a & NIBBLE_MASK) < (operandValue & NIBBLE_MASK)) {
        flags |= static_cast<unsigned>(Flag::HalfCarry);
      }
      if (a < operandValue) {
        flags |= static_cast<unsigned>(Flag::Carry);
      }
      storeResult = false;
      break;
  }

  const auto truncatedResult = static_cast<std::uint8_t>(result);
  if (truncatedResult == 0) {
    flags |= static_cast<unsigned>(Flag::Zero);
  }

  const auto finalA =
    storeResult ? truncatedResult : static_cast<std::uint8_t>(a);
  m_AF =
    static_cast<std::uint16_t>((static_cast<unsigned>(finalA) << 8U) | flags);
}

std::size_t
Cpu::aluR8()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto op =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x07U);
  const auto srcCode =
    static_cast<std::uint8_t>(static_cast<unsigned>(opcode) & 0x07U);

  std::size_t cycles = 1;
  std::uint8_t operand = 0;
  if (srcCode == REG_HL_INDIRECT) {
    operand = m_mmu.get().readByte(m_HL);
    cycles = 2; // NOLINT(readability-magic-numbers)
  } else {
    operand = getR8(srcCode);
  }

  applyAluOp(op, operand);

  m_PC += 1;
  return cycles;
}

std::size_t
Cpu::aluD8()
{
  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto op =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x07U);
  const auto operand = m_mmu.get().readByte(m_PC + 1);

  applyAluOp(op, operand);

  m_PC += 2;
  return 2; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::cbPrefixed()
{
  constexpr std::uint8_t groupRotateShift = 0x0;
  constexpr std::uint8_t groupBit = 0x1;
  constexpr std::uint8_t groupRes = 0x2;

  constexpr std::uint8_t opRlc = 0x0;
  constexpr std::uint8_t opRrc = 0x1;
  constexpr std::uint8_t opRl = 0x2;
  constexpr std::uint8_t opRr = 0x3;
  constexpr std::uint8_t opSla = 0x4;
  constexpr std::uint8_t opSra = 0x5;
  constexpr std::uint8_t opSwap = 0x6;

  constexpr std::uint8_t signBit = 0x80;
  constexpr std::uint8_t lowBit = 0x01;
  constexpr unsigned nibbleShift = 4U;

  const auto cbOpcode = m_mmu.get().readByte(m_PC + 1);
  const auto group =
    static_cast<std::uint8_t>((static_cast<unsigned>(cbOpcode) >> 6U) & 0x03U);
  const auto middle =
    static_cast<std::uint8_t>((static_cast<unsigned>(cbOpcode) >> 3U) & 0x07U);
  const auto regCode =
    static_cast<std::uint8_t>(static_cast<unsigned>(cbOpcode) & 0x07U);

  const bool isMemory = (regCode == REG_HL_INDIRECT);
  auto value = isMemory ? m_mmu.get().readByte(m_HL) : getR8(regCode);

  std::size_t cycles = isMemory ? 4 : 2; // NOLINT(readability-magic-numbers)

  if (group == groupBit) {
    const auto bitMask = 1U << middle;
    auto flags =
      static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Carry);
    flags |= static_cast<unsigned>(Flag::HalfCarry);
    if ((static_cast<unsigned>(value) & bitMask) == 0) {
      flags |= static_cast<unsigned>(Flag::Zero);
    }
    m_AF = static_cast<std::uint16_t>(
      (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);
    if (isMemory) {
      cycles = 3; // NOLINT(readability-magic-numbers)
    }
    m_PC += 2;
    return cycles;
  }

  if (group == groupRes) { // RES
    value =
      static_cast<std::uint8_t>(static_cast<unsigned>(value) & ~(1U << middle));
  } else if (group != groupRotateShift) { // SET
    value =
      static_cast<std::uint8_t>(static_cast<unsigned>(value) | (1U << middle));
  } else { // rotate/shift group
    const bool oldCarry = (m_AF & static_cast<std::uint16_t>(Flag::Carry)) != 0;
    bool newCarry = false;
    switch (middle) {
      case opRlc:
        newCarry = (value & signBit) != 0;
        value = static_cast<std::uint8_t>((static_cast<unsigned>(value) << 1U) |
                                          (newCarry ? 1U : 0U));
        break;
      case opRrc:
        newCarry = (value & lowBit) != 0;
        value = static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 1U) |
                                          (newCarry ? signBit : 0U));
        break;
      case opRl:
        newCarry = (value & signBit) != 0;
        value = static_cast<std::uint8_t>((static_cast<unsigned>(value) << 1U) |
                                          (oldCarry ? 1U : 0U));
        break;
      case opRr:
        newCarry = (value & lowBit) != 0;
        value = static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 1U) |
                                          (oldCarry ? signBit : 0U));
        break;
      case opSla:
        newCarry = (value & signBit) != 0;
        value = static_cast<std::uint8_t>(static_cast<unsigned>(value) << 1U);
        break;
      case opSra:
        newCarry = (value & lowBit) != 0;
        value =
          static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 1U) |
                                    (static_cast<unsigned>(value) & signBit));
        break;
      case opSwap:
        value = static_cast<std::uint8_t>(
          (static_cast<unsigned>(value) << nibbleShift) |
          (static_cast<unsigned>(value) >> nibbleShift));
        break;
      default: // SRL
        newCarry = (value & lowBit) != 0;
        value = static_cast<std::uint8_t>(static_cast<unsigned>(value) >> 1U);
        break;
    }

    unsigned flags = 0;
    if (value == 0) {
      flags |= static_cast<unsigned>(Flag::Zero);
    }
    if (middle != opSwap && newCarry) { // SWAP always clears Carry
      flags |= static_cast<unsigned>(Flag::Carry);
    }
    m_AF = static_cast<std::uint16_t>(
      (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);
  }

  if (isMemory) {
    m_mmu.get().writeByte(m_HL, value);
  } else {
    setR8(regCode, value);
  }

  m_PC += 2;
  return cycles;
}

std::size_t
Cpu::rotateA()
{
  constexpr std::uint8_t opRlca = 0x0;
  constexpr std::uint8_t opRrca = 0x1;
  constexpr std::uint8_t opRla = 0x2;
  constexpr std::uint8_t signBit = 0x80;
  constexpr std::uint8_t lowBit = 0x01;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto op =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 3U) & 0x03U);
  auto value = getR8(REG_A);
  const bool oldCarry = (m_AF & static_cast<std::uint16_t>(Flag::Carry)) != 0;
  bool newCarry = false;

  switch (op) {
    case opRlca:
      newCarry = (value & signBit) != 0;
      value = static_cast<std::uint8_t>((static_cast<unsigned>(value) << 1U) |
                                        (newCarry ? 1U : 0U));
      break;
    case opRrca:
      newCarry = (value & lowBit) != 0;
      value = static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 1U) |
                                        (newCarry ? signBit : 0U));
      break;
    case opRla:
      newCarry = (value & signBit) != 0;
      value = static_cast<std::uint8_t>((static_cast<unsigned>(value) << 1U) |
                                        (oldCarry ? 1U : 0U));
      break;
    default: // RRA
      newCarry = (value & lowBit) != 0;
      value = static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 1U) |
                                        (oldCarry ? signBit : 0U));
      break;
  }

  setR8(REG_A, value);

  // Unlike the CB-prefixed RLC/RRC/RL/RR, these always clear Zero regardless
  // of the result.
  const unsigned flags = newCarry ? static_cast<unsigned>(Flag::Carry) : 0;
  m_AF = static_cast<std::uint16_t>(
    (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);

  m_PC += 1;
  return 1;
}

std::size_t
Cpu::addhlr16()
{
  constexpr unsigned bits0To11Mask = 0x0FFF;
  constexpr unsigned word16Overflow = 0x10000;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto r16 =
    static_cast<std::uint8_t>((static_cast<unsigned>(opcode) >> 4U) & 0x03U);

  std::uint16_t operand = 0;
  switch (r16) {
    case 0x0:
      operand = m_BC;
      break;
    case 0x1:
      operand = m_DE;
      break;
    case 0x2:
      operand = m_HL;
      break;
    default: // 0x3
      operand = m_SP;
      break;
  }

  const auto hl = static_cast<unsigned>(m_HL);
  const auto op = static_cast<unsigned>(operand);
  const unsigned result = hl + op;

  // Zero is preserved (this instruction never sets/clears it); Subtract is
  // reset; Half-Carry/Carry check bit 11/15, not bit 3/7 like 8-bit ADD.
  auto flags = static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Zero);
  if (((hl & bits0To11Mask) + (op & bits0To11Mask)) > bits0To11Mask) {
    flags |= static_cast<unsigned>(Flag::HalfCarry);
  }
  if (result >= word16Overflow) {
    flags |= static_cast<unsigned>(Flag::Carry);
  }

  m_HL = static_cast<std::uint16_t>(result);
  m_AF = static_cast<std::uint16_t>(
    (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);

  m_PC += 1;
  return 2;
}

std::size_t
Cpu::jphl()
{
  m_PC = m_HL;
  return 1;
}

std::size_t
Cpu::ldA16Sp()
{
  const auto address = m_mmu.get().readWord(m_PC + 1);
  m_mmu.get().writeWord(address, m_SP);

  m_PC += 3;
  return 5; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::addSpHlE8()
{
  constexpr std::uint8_t opcodeAddSp = 0xE8;
  constexpr unsigned byteOverflow = 0x100;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const auto immediate = m_mmu.get().readByte(m_PC + 1);
  const auto offset = static_cast<std::int8_t>(immediate);

  // H/C are computed as an unsigned 8-bit add of SP's low byte with the
  // immediate's raw byte value, not from the signed 16-bit sum below - a
  // well-known GB CPU quirk shared by ADD SP,e8 and LD HL,SP+e8.
  const auto lowSp = static_cast<unsigned>(m_SP) & LOW_BYTE_MASK;
  const auto operand = static_cast<unsigned>(immediate);
  unsigned flags = 0;
  if (((lowSp & NIBBLE_MASK) + (operand & NIBBLE_MASK)) > NIBBLE_MASK) {
    flags |= static_cast<unsigned>(Flag::HalfCarry);
  }
  if ((lowSp + operand) >= byteOverflow) {
    flags |= static_cast<unsigned>(Flag::Carry);
  }

  const auto result =
    static_cast<std::uint16_t>(static_cast<int>(m_SP) + offset);
  if (opcode == opcodeAddSp) {
    m_SP = result;
  } else {
    m_HL = result;
  }

  // Z and N are always cleared; A is untouched by either opcode.
  m_AF = static_cast<std::uint16_t>(
    (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);

  m_PC += 2;
  return opcode == opcodeAddSp ? 4 : 3; // NOLINT(readability-magic-numbers)
}

std::size_t
Cpu::ldSpHl()
{
  m_SP = m_HL;
  m_PC += 1;
  return 2;
}

std::size_t
Cpu::daaCplScfCcf()
{
  constexpr std::uint8_t opDaa = 0x27;
  constexpr std::uint8_t opCpl = 0x2F;
  constexpr std::uint8_t opScf = 0x37;
  constexpr unsigned daaAddHalf = 0x06;
  constexpr unsigned daaAddFull = 0x60;
  constexpr unsigned daaLowNibbleThreshold = 0x09;
  constexpr unsigned daaFullThreshold = 0x99;

  const auto opcode = m_mmu.get().readByte(m_PC);
  const bool subtractSet =
    (m_AF & static_cast<std::uint16_t>(Flag::Subtract)) != 0;
  const bool halfCarrySet =
    (m_AF & static_cast<std::uint16_t>(Flag::HalfCarry)) != 0;
  const bool carrySet = (m_AF & static_cast<std::uint16_t>(Flag::Carry)) != 0;
  const auto zeroFlagBits =
    static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Zero);

  unsigned flags = 0;

  if (opcode == opDaa) {
    auto a = static_cast<unsigned>(getR8(REG_A));
    bool newCarry = carrySet;
    if (!subtractSet) {
      if (carrySet || a > daaFullThreshold) {
        a += daaAddFull;
        newCarry = true;
      }
      if (halfCarrySet || (a & NIBBLE_MASK) > daaLowNibbleThreshold) {
        a += daaAddHalf;
      }
    } else {
      if (carrySet) {
        a -= daaAddFull;
      }
      if (halfCarrySet) {
        a -= daaAddHalf;
      }
    }
    setR8(REG_A, static_cast<std::uint8_t>(a));
    flags = static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Subtract);
    if (static_cast<std::uint8_t>(a) == 0) {
      flags |= static_cast<unsigned>(Flag::Zero);
    }
    if (newCarry) {
      flags |= static_cast<unsigned>(Flag::Carry);
    }
  } else if (opcode == opCpl) {
    setR8(REG_A, static_cast<std::uint8_t>(~getR8(REG_A)));
    flags = zeroFlagBits | static_cast<unsigned>(Flag::Subtract) |
            static_cast<unsigned>(Flag::HalfCarry) |
            (static_cast<unsigned>(m_AF) & static_cast<unsigned>(Flag::Carry));
  } else if (opcode == opScf) {
    flags = zeroFlagBits | static_cast<unsigned>(Flag::Carry);
  } else { // CCF
    flags = zeroFlagBits;
    if (!carrySet) {
      flags |= static_cast<unsigned>(Flag::Carry);
    }
  }

  m_AF = static_cast<std::uint16_t>(
    (static_cast<unsigned>(m_AF) & HIGH_BYTE_MASK) | flags);

  m_PC += 1;
  return 1;
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