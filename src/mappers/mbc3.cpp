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

// Real MBC3 hardware only implements a subset of the bits in each RTC
// register - seconds/minutes are 6-bit counters, hours a 5-bit counter, DL
// is a full 8-bit day-counter byte, and DH exposes only the day-counter high
// bit (0), halt (6), and day-carry (7) bits. Writes to the remaining bits
// are ignored and always read back as 0. Indexed the same way as
// m_rtcRegisters/RTC_S../RTC_DH above.
static constexpr std::array<std::uint8_t, 5> REGISTER_MASKS = { 0x3F,
                                                                0x3F,
                                                                0x1F,
                                                                0xFF,
                                                                0xC1 };

namespace {
// Advances one seconds/minutes/hours counter by a tick and reports whether
// the next counter up should also tick. Real MBC3 hardware distinguishes
// two kinds of rollover: a "valid" one, triggered only when the field held
// exactly its normal maximum (validMax) beforehand, which resets to 0 *and*
// carries into the next unit; and an "invalid" one, reached only when the
// field already held an out-of-range value (e.g. seconds/minutes written as
// 60-63, hours written as 24-31) and increments past the counter's bit
// width (hardMax), which resets to 0 *without* carrying. A field sitting on
// an invalid value that hasn't yet hit hardMax just keeps counting up
// normally - see rtc3test's "range tests" subtest, which exercises exactly
// this quirk.
[[nodiscard]] bool
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
tickField(std::uint8_t& field, std::uint8_t validMax, std::uint8_t hardMax)
{
  if (field == validMax) {
    field = 0;
    return true;
  }
  ++field;
  if (field > hardMax) {
    field = 0;
  }
  return false;
}
}
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
  return Mapper::readRam(address, m_ramBank);
}

void
Mbc3Mapper::writeRam(std::uint16_t address, std::uint8_t value)
{
  if (!m_ramAndTimerEnabled) {
    return;
  }
  if (m_selectedRtc.has_value()) {
    switch (*m_selectedRtc) {
      case mbc3::RTC_S:
        m_rtc.seconds = value & mbc3::REGISTER_MASKS.at(mbc3::RTC_S);
        // Real hardware resets the sub-second prescaler on any write to the
        // seconds register - writes to the other RTC registers leave it
        // running in place. See rtc3test's "sub-second writes" subtest.
        m_rtc.tCycles = 0;
        break;
      case mbc3::RTC_M:
        m_rtc.minutes = value & mbc3::REGISTER_MASKS.at(mbc3::RTC_M);
        break;
      case mbc3::RTC_H:
        m_rtc.hours = value & mbc3::REGISTER_MASKS.at(mbc3::RTC_H);
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
    m_rtcRegisters.at(*m_selectedRtc) =
      value & mbc3::REGISTER_MASKS.at(*m_selectedRtc);
    return;
  }
  Mapper::writeRam(address, m_ramBank, value);
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
  // The sub-second prescaler itself is gated by halt too - real hardware
  // freezes it in place while halted rather than letting it keep running
  // and only masking the S/M/H/day fields, so it resumes from the exact
  // sub-second position it was halted at. See rtc3test's "sub-second
  // writes" subtest, specifically the "RTC off" case.
  if (halt) {
    return;
  }
  ++tCycles;
  if (tCycles >= mbc3::T_CYCLES_PER_SECOND) {
    tCycles = 0;
    if (mbc3::tickField(seconds, 59, 63) && mbc3::tickField(minutes, 59, 63) &&
        mbc3::tickField(hours, 23, 31)) {
      ++days;
      if (days > 511) {
        days = 0;
        dayCarry = true;
      }
    }
  }
}
}
