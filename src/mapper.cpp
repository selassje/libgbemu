module;
#include <utility>
module gbemu;

namespace {

constexpr unsigned MBC1_BANK_HIGH_SHIFT = 5U;
constexpr std::uint16_t MBC1_ROM_BANK_REGISTER_START = 0x2000;
constexpr std::uint16_t MBC1_RAM_BANK_REGISTER_START = 0x4000;
constexpr std::uint16_t MBC1_BANKING_MODE_REGISTER_START = 0x6000;
constexpr unsigned MBC1_ROM_BANK_MASK = 0x1FU;
constexpr unsigned MBC1_BANK_HIGH_MASK = 0x03U;
constexpr unsigned MBC1_BANKING_MODE_MASK = 0x01U;
constexpr std::uint16_t EXT_RAM_START = 0xA000;

}
namespace gbemu {
namespace mbc3 {
static constexpr std::uint64_t T_CYCLES_PER_SECOND = 4194304;
static constexpr std::uint16_t RTC_S = 0x00;
static constexpr std::uint16_t RTC_M = 0x01;
static constexpr std::uint16_t RTC_H = 0x2;
static constexpr std::uint16_t RTC_DL = 0x3;
static constexpr std::uint16_t RTC_DH = 0x4;
};

Mapper::Mapper(std::span<const std::uint8_t> rom)
  : m_rom(rom.begin(), rom.end())
{
}

std::uint8_t
Mapper::readRam(std::uint16_t address) const
{
  return m_ram.at(address - EXT_RAM_START);
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  m_ram.at(address - EXT_RAM_START) = value;
}

RomOnlyMapper::RomOnlyMapper(std::span<const std::uint8_t> rom)
  : Mapper(rom)
{
}

std::uint8_t
RomOnlyMapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    return romByte(address);
  }
  // No register ever changes which bank this reads (writeRom() is a
  // no-op) - always bank 1, the same fixed value Mmu's pre-refactor
  // m_switchableRomBank permanently held for a non-MBC1 cartridge.
  const auto bank = std::size_t{ 1 } % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
RomOnlyMapper::writeRom(std::uint16_t /*address*/, std::uint8_t /*value*/)
{
  // No registers - ROM ONLY cartridges have no bank switching.
}

void
RomOnlyMapper::reset()
{
  resetRam();
}

void
RomOnlyMapper::serialize(SaveStateWriter& writer) const
{
  serializeRam(writer);
}

void
RomOnlyMapper::deserialize(SaveStateReader& reader)
{
  deserializeRam(reader);
}

Mbc1Mapper::Mbc1Mapper(std::span<const std::uint8_t> rom)
  : Mapper(rom)
{
}

std::size_t
Mbc1Mapper::currentRomBank() const
{
  return (static_cast<std::size_t>(m_bankHigh) << MBC1_BANK_HIGH_SHIFT) |
         m_romBankLow;
}

std::uint8_t
Mbc1Mapper::readRom(std::uint16_t address) const
{
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  if (address < KB16) {
    // Advanced banking mode (m_bankingMode) remaps bank 0 too, using just
    // the high bits - a genuine real-hardware quirk (used by multicarts
    // and to reach banks 0x20/0x40/0x60, which m_romBankLow alone can
    // never select - see writeRom()), not an emulator invention.
    std::size_t bank = 0;
    if (m_bankingMode) {
      bank = static_cast<std::size_t>(m_bankHigh) << MBC1_BANK_HIGH_SHIFT;
    }
    bank %= bankCount;
    return romByte((bank * KB16) + address);
  }
  const auto bank = currentRomBank() % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc1Mapper::writeRom(std::uint16_t address, std::uint8_t value)
{
  const auto unsignedValue = static_cast<unsigned>(value);
  if (address < MBC1_ROM_BANK_REGISTER_START) {
    // RAM-enable register (0x0000-0x1FFF) - not modeled yet (see Mapper's
    // own comment on RAM always being accessible regardless of any
    // header/register gating).
    return;
  }
  if (address < MBC1_RAM_BANK_REGISTER_START) {
    m_romBankLow =
      static_cast<std::uint8_t>(unsignedValue & MBC1_ROM_BANK_MASK);
    if (m_romBankLow == 0) {
      m_romBankLow = 1;
    }
    return;
  }
  if (address < MBC1_BANKING_MODE_REGISTER_START) {
    m_bankHigh = static_cast<std::uint8_t>(unsignedValue & MBC1_BANK_HIGH_MASK);
    return;
  }
  m_bankingMode = (unsignedValue & MBC1_BANKING_MODE_MASK) != 0;
}

void
Mbc1Mapper::reset()
{
  resetRam();
  m_romBankLow = 1;
  m_bankHigh = 0;
  m_bankingMode = false;
}

void
Mbc1Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBankLow);
  writer.writeU8(m_bankHigh);
  writer.writeBool(m_bankingMode);
  serializeRam(writer);
}

void
Mbc1Mapper::deserialize(SaveStateReader& reader)
{
  m_romBankLow = reader.readU8();
  m_bankHigh = reader.readU8();
  m_bankingMode = reader.readBool();
  deserializeRam(reader);
}

Mbc3Mapper::Mbc3Mapper(std::span<const std::uint8_t> rom)
  : Mapper(rom) // Initialize m_romBank to 1
{
}

std::uint8_t
Mbc3Mapper::readRom(std::uint16_t address) const
{
  if (address < KB16) {
    return romByte(address);
  }
  const auto bankCount = std::max<std::size_t>(1, romSize() / KB16);
  const auto bank = m_romBank % bankCount;
  return romByte((bank * KB16) + (address - KB16));
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Mbc3Mapper::writeRom(std::uint16_t address, std::uint8_t value)
{
  if (address < 0x2000) {
    if (value == 0x0A) {
      m_ramAndTimerEnabled = true;
    } else if (value == 0x00) {
      m_ramAndTimerEnabled = false;
    }
  } else if (address < 0x4000) {
    m_romBank = value;
    if (m_romBank == 0) {
      m_romBank = 1;
    }
  } else if (address < 0x6000) {
    if (value <= 0x03) {
      m_ramBank = value;
      m_selectedRtc = std::nullopt;
    } else if (value >= 0x08 && value <= 0x0C) {
      m_selectedRtc = static_cast<std::size_t>(value - 0x08);
    }
  } else if (address < 0x8000) {
    if (m_lastLatchValue == 0x00 && value == 0x01) {
      m_rtcRegisters.at(0) = m_rtc.seconds;
      m_rtcRegisters.at(1) = m_rtc.minutes;
      m_rtcRegisters.at(2) = m_rtc.hours;
      m_rtcRegisters.at(3) = static_cast<std::uint8_t>(m_rtc.days & 0xFFU);
      m_rtcRegisters.at(4) = static_cast<std::uint8_t>(m_rtc.days >> 8U);
      const std::uint8_t dayCarryBit = m_rtc.dayCarry ? 0x80U : 0x00U;
      const std::uint8_t haltBit = m_rtc.halt ? 0x40U : 0x00U;
      m_rtcRegisters.at(4) |= static_cast<std::uint8_t>(dayCarryBit | haltBit);
    }
    m_lastLatchValue = value;
  }
}

std::uint8_t
Mbc3Mapper::readRam(std::uint16_t address) const
{
  if (!m_ramAndTimerEnabled) {
    return 0xFF;
  }
  if (m_selectedRtc.has_value()) {
    return m_rtcRegisters.at(*m_selectedRtc);
  }
  return Mapper::readRam(
    static_cast<std::uint16_t>(address + (m_ramBank * RAM_BANK_SIZE)));
}

void
Mbc3Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  if (!m_ramAndTimerEnabled) {
    return;
  }
  if (m_selectedRtc.has_value()) {
    m_rtcRegisters.at(*m_selectedRtc) = value;
    switch (*m_selectedRtc) {
      case mbc3::RTC_S:
        m_rtc.seconds = value;
        break;
      case mbc3::RTC_M:
        m_rtc.minutes = value;
        break;
      case mbc3::RTC_H:
        m_rtc.hours = value;
        break;
      case mbc3::RTC_DL:
        m_rtc.days = (m_rtc.days & 0x100U) | static_cast<std::uint16_t>(value);
        break;
      case mbc3::RTC_DH: {
        const auto dayHighBit = static_cast<std::uint16_t>(value & 0x01U);
        const auto shiftedDayHighBit =
          static_cast<std::uint16_t>(dayHighBit << 8U);
        m_rtc.days = (m_rtc.days & 0xFFU) | shiftedDayHighBit;
        m_rtc.halt = (value & 0x40U) != 0;
        m_rtc.dayCarry = (value & 0x80U) != 0;
      } break;
      default:
        std::unreachable();
    }
    return;
  }
  Mapper::writeRam(
    static_cast<std::uint16_t>(address + (m_ramBank * RAM_BANK_SIZE)), value);
}

void
Mbc3Mapper::reset()
{
  resetRam();
  m_romBank = 1;
  m_ramBank = 0;
}

void
Mbc3Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBank);
  writer.writeU8(m_ramBank);
  serializeRam(writer);
}

void
Mbc3Mapper::deserialize(SaveStateReader& reader)
{
  m_romBank = reader.readU8();
  m_ramBank = reader.readU8();
  deserializeRam(reader);
}

void
Mbc3Mapper::runNextTCycle()
{
  m_rtc.runNextTCycle();
}

void
Mbc3Mapper::RealTimeClock::runNextTCycle()
{
  ++tCycles;
  if (tCycles >= mbc3::T_CYCLES_PER_SECOND) {
    tCycles = 0;
    if (!halt) {
      ++seconds;
      if (seconds >= 60) {
        seconds = 0;
        ++minutes;
        if (minutes >= 60) {
          minutes = 0;
          ++hours;
          if (hours >= 24) {
            hours = 0;
            ++days;
            if (days > 511) {
              days = 0;
              dayCarry = true;
            }
          }
        }
      }
    }
  }
}
}