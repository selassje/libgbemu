module;
#include <utility>
module gbemu;

namespace gbemu {
namespace mbc3 {
static constexpr std::uint64_t T_CYCLES_PER_SECOND = 4194304;
static constexpr std::uint16_t RTC_S = 0x00;
static constexpr std::uint16_t RTC_M = 0x01;
static constexpr std::uint16_t RTC_H = 0x2;
static constexpr std::uint16_t RTC_DL = 0x3;
static constexpr std::uint16_t RTC_DH = 0x4;
};

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
  m_ramAndTimerEnabled = false;
  m_selectedRtc = std::nullopt;
  m_rtcRegisters = {};
  m_lastLatchValue = 0xFF;
  m_rtc = RealTimeClock{};
}

void
Mbc3Mapper::serialize(SaveStateWriter& writer) const
{
  writer.writeU8(m_romBank);
  writer.writeU8(m_ramBank);
  writer.writeBool(m_ramAndTimerEnabled);

  writer.writeBool(m_selectedRtc.has_value());
  if (m_selectedRtc) {
    writer.writeSize(*m_selectedRtc);
  }
  writer.writeBytes(m_rtcRegisters);
  writer.writeU8(m_lastLatchValue);

  writer.writeU8(m_rtc.seconds);
  writer.writeU8(m_rtc.minutes);
  writer.writeU8(m_rtc.hours);
  writer.writeU16(m_rtc.days);
  writer.writeBool(m_rtc.dayCarry);
  writer.writeBool(m_rtc.halt);
  writer.writeU64(m_rtc.tCycles);

  serializeRam(writer);
}

void
Mbc3Mapper::deserialize(SaveStateReader& reader)
{
  m_romBank = reader.readU8();
  m_ramBank = reader.readU8();
  m_ramAndTimerEnabled = reader.readBool();

  if (reader.readBool()) {
    m_selectedRtc = reader.readSize();
  } else {
    m_selectedRtc = std::nullopt;
  }
  reader.readBytes(m_rtcRegisters);
  m_lastLatchValue = reader.readU8();

  m_rtc.seconds = reader.readU8();
  m_rtc.minutes = reader.readU8();
  m_rtc.hours = reader.readU8();
  m_rtc.days = reader.readU16();
  m_rtc.dayCarry = reader.readBool();
  m_rtc.halt = reader.readBool();
  m_rtc.tCycles = reader.readU64();

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
