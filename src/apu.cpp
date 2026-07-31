module gbemu;

namespace {

constexpr std::uint32_t CLOCK_RATE_HZ = 4194304;

// DIV bit 4 - bit 12 of the full 16-bit counter Mmu::divCounter() exposes.
// The frame sequencer steps on a 1->0 transition of this bit, not on a
// fixed T-cycle divisor - see Apu::runNextTCycle().
constexpr std::uint16_t FRAME_SEQUENCER_BIT_MASK = 0b0001'0000'0000'0000U;

// NR11/NR21 bits 7-6 (duty) select one of these 4 waveforms - each row is
// one full 8-step cycle read left to right, 1 = high, 0 = low. Duty 3 is
// the exact element-wise complement of duty 1 (25% and 75% are
// complementary duty cycles), a useful sanity check on these values.
constexpr std::array<std::array<std::uint8_t, 8>, 4> DUTY_CYCLES = { {
  { 0, 0, 0, 0, 0, 0, 0, 1 }, // 12.5%
  { 1, 0, 0, 0, 0, 0, 0, 1 }, // 25%
  { 1, 0, 0, 0, 0, 1, 1, 1 }, // 50%
  { 0, 1, 1, 1, 1, 1, 1, 0 }, // 75%
} };

}

namespace gbemu {

void
Apu::startFrame()
{
  m_sampleCount = 0;
}

void
Apu::runNextTCycle(std::uint16_t divCounter)
{
  const bool currentFrameSequencerBit =
    (divCounter & FRAME_SEQUENCER_BIT_MASK) != 0;
  if (m_previousFrameSequencerBit && !currentFrameSequencerBit) {
    constexpr std::uint8_t frameSequencerStepCount = 8;
    m_frameSequencerStep = static_cast<std::uint8_t>(
      (m_frameSequencerStep + 1) % frameSequencerStepCount);
  }
  m_previousFrameSequencerBit = currentFrameSequencerBit;

  m_sampleAccumulator += static_cast<std::uint32_t>(SAMPLE_RATE);
  if (m_sampleAccumulator < CLOCK_RATE_HZ) {
    return;
  }
  m_sampleAccumulator -= CLOCK_RATE_HZ;

  // Placeholder silence - this only wires up the T-cycle-driven sample
  // timing and per-frame buffer; actual channel synthesis/mixing isn't
  // implemented yet.
  m_buffer.at(m_sampleCount++) = 0.0F;
  m_buffer.at(m_sampleCount++) = 0.0F;
}

void
Apu::writeEnvelope(Envelope& envelope, std::uint8_t value)
{
  constexpr unsigned initialVolumeShift = 4U;
  constexpr unsigned initialVolumeMask = 0b1111U;
  constexpr unsigned increaseBit = 0b0000'1000U;
  constexpr unsigned paceMask = 0b111U;

  const auto unsignedValue = static_cast<unsigned>(value);
  envelope.initialVolume = static_cast<std::uint8_t>(
    (unsignedValue >> initialVolumeShift) & initialVolumeMask);
  envelope.increase = (unsignedValue & increaseBit) != 0;
  envelope.pace = static_cast<std::uint8_t>(unsignedValue & paceMask);
}

// address/value is this file's (and Mmu::writeByte()'s) established
// register-write shape.
void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Apu::writeRegister(std::uint16_t address, std::uint8_t value)
{
  if (address == regs::NR10) {
    constexpr unsigned paceShift = 4U;
    constexpr unsigned paceMask = 0b111U;
    constexpr unsigned directionBit = 0b0000'1000U;
    constexpr unsigned shiftMask = 0b111U;

    const auto unsignedValue = static_cast<unsigned>(value);
    m_pulse1.sweep.pace =
      static_cast<std::uint8_t>((unsignedValue >> paceShift) & paceMask);
    m_pulse1.sweep.isIncrease = (unsignedValue & directionBit) == 0;
    m_pulse1.sweep.shift = static_cast<std::uint8_t>(unsignedValue & shiftMask);
    return;
  }

  if (address == regs::NR11 || address == regs::NR21) {
    constexpr unsigned dutyShift = 6U;
    constexpr unsigned dutyMask = 0b11U;
    constexpr unsigned lengthMask = 0b0011'1111U;

    const auto unsignedValue = static_cast<unsigned>(value);
    PulseChannel& pulse = (address == regs::NR11) ? m_pulse1 : m_pulse2;
    pulse.duty =
      static_cast<std::uint8_t>((unsignedValue >> dutyShift) & dutyMask);
    pulse.lengthTimer = static_cast<std::uint8_t>(unsignedValue & lengthMask);
    return;
  }

  if (address == regs::NR12 || address == regs::NR22 || address == regs::NR42) {
    Envelope* envelope = nullptr;
    bool* enabled = nullptr;
    if (address == regs::NR12) {
      envelope = &m_pulse1.envelope;
      enabled = &m_pulse1.enabled;
    } else if (address == regs::NR22) {
      envelope = &m_pulse2.envelope;
      enabled = &m_pulse2.enabled;
    } else {
      envelope = &m_noise.envelope;
      enabled = &m_noise.enabled;
    }
    writeEnvelope(*envelope, value);
    // "Setting bits 3-7 of this register all to 0 ... turns the DAC off
    // (and thus, the channel as well)".
    if (!isDacEnabled(*envelope)) {
      *enabled = false;
    }
    return;
  }

  if (address == regs::NR13 || address == regs::NR23) {
    constexpr unsigned periodHighBitsMask = 0b0000'0111'0000'0000U;

    PulseChannel& pulse = (address == regs::NR13) ? m_pulse1 : m_pulse2;
    pulse.period = static_cast<std::uint16_t>(
      (static_cast<unsigned>(pulse.period) & periodHighBitsMask) |
      static_cast<unsigned>(value));
    return;
  }

  if (address == regs::NR14 || address == regs::NR24) {
    constexpr unsigned triggerBit = 0b1000'0000U;
    constexpr unsigned lengthEnableBit = 0b0100'0000U;
    constexpr unsigned periodHighMask = 0b111U;
    constexpr unsigned periodHighShift = 8U;
    constexpr unsigned periodLowBitsMask = 0x00FFU;

    const auto unsignedValue = static_cast<unsigned>(value);
    PulseChannel& pulse = (address == regs::NR14) ? m_pulse1 : m_pulse2;
    pulse.isLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
    pulse.period = static_cast<std::uint16_t>(
      (static_cast<unsigned>(pulse.period) & periodLowBitsMask) |
      ((unsignedValue & periodHighMask) << periodHighShift));
    if ((unsignedValue & triggerBit) != 0) {
      pulse.enabled = isDacEnabled(pulse.envelope);
    }
    return;
  }

  if (address == regs::NR30) {
    constexpr unsigned dacEnabledBit = 0b1000'0000U;
    m_wave.dacEnabled = (static_cast<unsigned>(value) & dacEnabledBit) != 0;
    if (!m_wave.dacEnabled) {
      m_wave.enabled = false;
    }
    return;
  }

  if (address == regs::NR31) {
    m_wave.lengthTimer = value;
    return;
  }

  if (address == regs::NR32) {
    constexpr unsigned outputLevelShift = 5U;
    constexpr unsigned outputLevelMask = 0b11U;
    m_wave.outputLevel = static_cast<std::uint8_t>(
      (static_cast<unsigned>(value) >> outputLevelShift) & outputLevelMask);
    return;
  }

  if (address == regs::NR33) {
    constexpr unsigned periodHighBitsMask = 0b0000'0111'0000'0000U;
    m_wave.period = static_cast<std::uint16_t>(
      (static_cast<unsigned>(m_wave.period) & periodHighBitsMask) |
      static_cast<unsigned>(value));
    return;
  }

  if (address == regs::NR34) {
    constexpr unsigned triggerBit = 0b1000'0000U;
    constexpr unsigned lengthEnableBit = 0b0100'0000U;
    constexpr unsigned periodHighMask = 0b111U;
    constexpr unsigned periodHighShift = 8U;
    constexpr unsigned periodLowBitsMask = 0x00FFU;

    const auto unsignedValue = static_cast<unsigned>(value);
    m_wave.isLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
    m_wave.period = static_cast<std::uint16_t>(
      (static_cast<unsigned>(m_wave.period) & periodLowBitsMask) |
      ((unsignedValue & periodHighMask) << periodHighShift));
    if ((unsignedValue & triggerBit) != 0) {
      m_wave.enabled = m_wave.dacEnabled;
    }
    return;
  }

  if (address == regs::NR41) {
    constexpr unsigned lengthMask = 0b0011'1111U;
    m_noise.lengthTimer =
      static_cast<std::uint8_t>(static_cast<unsigned>(value) & lengthMask);
    return;
  }

  if (address == regs::NR43) {
    constexpr unsigned clockShiftShift = 4U;
    constexpr unsigned clockShiftMask = 0b1111U;
    constexpr unsigned narrowLfsrBit = 0b0000'1000U;
    constexpr unsigned clockDividerMask = 0b111U;

    const auto unsignedValue = static_cast<unsigned>(value);
    m_noise.clockShift = static_cast<std::uint8_t>(
      (unsignedValue >> clockShiftShift) & clockShiftMask);
    m_noise.narrowLfsr = (unsignedValue & narrowLfsrBit) != 0;
    m_noise.clockDivider =
      static_cast<std::uint8_t>(unsignedValue & clockDividerMask);
    return;
  }

  if (address == regs::NR44) {
    constexpr unsigned triggerBit = 0b1000'0000U;
    constexpr unsigned lengthEnableBit = 0b0100'0000U;

    const auto unsignedValue = static_cast<unsigned>(value);
    m_noise.isLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
    if ((unsignedValue & triggerBit) != 0) {
      m_noise.enabled = isDacEnabled(m_noise.envelope);
    }
    return;
  }

  if (address == regs::NR52) {
    constexpr unsigned powerBit = 0b1000'0000U;
    m_powered = (static_cast<unsigned>(value) & powerBit) != 0;
    if (!m_powered) {
      // Mirrors Mmu's own NR10-NR51 byte-range clearing on power-off - keep
      // Apu's channel state and Mmu's raw register bytes consistent with
      // each other.
      m_pulse1 = PulseChannel1{};
      m_pulse2 = PulseChannel{};
      m_wave = WaveChannel{};
      m_noise = NoiseChannel{};
    }
    return;
  }
}

std::uint8_t
Apu::readRegister(std::uint16_t address) const
{
  if (address == regs::NR52) {
    constexpr unsigned powerBit = 0b1000'0000U;
    constexpr unsigned unusedBits = 0b0111'0000U;
    constexpr unsigned ch1Bit = 0b0000'0001U;
    constexpr unsigned ch2Bit = 0b0000'0010U;
    constexpr unsigned ch3Bit = 0b0000'0100U;
    constexpr unsigned ch4Bit = 0b0000'1000U;

    unsigned result = unusedBits;
    if (m_powered) {
      result |= powerBit;
    }
    if (m_pulse1.enabled) {
      result |= ch1Bit;
    }
    if (m_pulse2.enabled) {
      result |= ch2Bit;
    }
    if (m_wave.enabled) {
      result |= ch3Bit;
    }
    if (m_noise.enabled) {
      result |= ch4Bit;
    }
    return static_cast<std::uint8_t>(result);
  }
  return 0;
}

}
