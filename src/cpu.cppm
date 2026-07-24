export module gbemu:cpu;

import std;

namespace gbemu {

class Cpu // NOLINT(misc-use-internal-linkage)
{
public:
  Cpu() = default;

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
};

}
