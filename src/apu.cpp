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
    static constexpr std::uint8_t frameSequencerStepCount = 8;
    m_frameSequencerStep = static_cast<std::uint8_t>(
      (m_frameSequencerStep + 1) % frameSequencerStepCount);

    switch (m_frameSequencerStep) {
      // CH1 period sweep: steps 2/6 (128 Hz) - falls through to also clock
      // length, same as steps 0/4.
      case 2:
      case 6:
        m_pulse1.clockSweep();
        [[fallthrough]];
      // Length: steps 0/2/4/6 (256 Hz - every other step).
      case 0:
      case 4:
        m_pulse1.clockLength();
        m_pulse2.clockLength();
        m_wave.clockLength();
        m_noise.clockLength();
        break;
      // Envelope: step 7 only (64 Hz).
      case 7:
        m_pulse1.clockEnvelope();
        m_pulse2.clockEnvelope();
        m_noise.clockEnvelope();
        break;
      default:
        break;
    }
  }
  m_previousFrameSequencerBit = currentFrameSequencerBit;

  // Every T-cycle, not gated behind the sample-timing accumulator below -
  // the period counters these drive need a real T-cycle tick to produce
  // the correct frequency, not the ~95x slower resampled rate.
  m_pulse1.runNextTCycle();
  m_pulse2.runNextTCycle();
  m_wave.runNextTCycle();
  m_noise.runNextTCycle();

  m_sampleAccumulator += static_cast<std::uint32_t>(SAMPLE_RATE);
  if (m_sampleAccumulator < CLOCK_RATE_HZ) {
    return;
  }
  m_sampleAccumulator -= CLOCK_RATE_HZ;

  // TODO: mix() itself is still a stub (returns silence) and applyHpf() is
  // a pass-through - see each one's own TODO for what's missing. Kept as
  // separate statements (not applyHpf(mix())) so the pre-filter mixed
  // output stays inspectable on its own.
  const auto mixed = mix();
  const auto [left, right] = applyHpf(mixed);
  m_buffer.at(m_sampleCount++) = left;
  m_buffer.at(m_sampleCount++) = right;
}

void
Apu::writeEnvelope(EnvelopeConfig& envelope, std::uint8_t value)
{
  static constexpr unsigned initialVolumeShift = 4U;
  static constexpr unsigned initialVolumeMask = 0b1111U;
  static constexpr unsigned increaseBit = 0b0000'1000U;
  static constexpr unsigned paceMask = 0b111U;

  const auto unsignedValue = static_cast<unsigned>(value);
  envelope.initialVolume = static_cast<std::uint8_t>(
    (unsignedValue >> initialVolumeShift) & initialVolumeMask);
  envelope.increase = (unsignedValue & increaseBit) != 0;
  envelope.pace = static_cast<std::uint8_t>(unsignedValue & paceMask);
}

float
Apu::toDacOutput(std::uint8_t amplitude)
{
  static constexpr float divisor = 7.5F;
  return (static_cast<float>(amplitude) / divisor) - 1.0F;
}

std::pair<float, float>
Apu::mix() const
{
  // TODO: not implemented yet - still missing summing
  // toDacOutput(m_pulse1.playback.output)/m_pulse2/m_wave/m_noise per NR51
  // panning. NR50 master volume is applied below (a value of 0 is volume
  // 1/8, not silence - the amplifier never mutes a non-silent input), but
  // there's nothing but silence to scale until panning exists.
  float left = 0.0F;
  float right = 0.0F;

  static constexpr float maxVolume = 8.0F;
  left *= static_cast<float>(m_leftVolume + 1) / maxVolume;
  right *= static_cast<float>(m_rightVolume + 1) / maxVolume;

  return { left, right };
}

// TODO: not implemented yet - pass-through until the high-pass filter
// itself exists (see the class comment on what real hardware does here).
// Deliberately not static despite the stub body not touching instance
// state: the real filter will need to (previous input/output per
// channel), so the signature is already what it'll need to be.
std::pair<float, float>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
Apu::applyHpf(std::pair<float, float> input)
{
  return input;
}

// address/value is this file's (and Mmu::writeByte()'s) established
// register-write shape.
void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
Apu::writeRegister(std::uint16_t address, std::uint8_t value)
{
  switch (address) {
    case regs::NR10: {
      static constexpr unsigned paceShift = 4U;
      static constexpr unsigned paceMask = 0b111U;
      static constexpr unsigned directionBit = 0b0000'1000U;
      static constexpr unsigned shiftMask = 0b111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_pulse1.sweep.pace =
        static_cast<std::uint8_t>((unsignedValue >> paceShift) & paceMask);
      m_pulse1.sweep.isIncrease = (unsignedValue & directionBit) == 0;
      m_pulse1.sweep.shift =
        static_cast<std::uint8_t>(unsignedValue & shiftMask);
      break;
    }

    case regs::NR11:
    case regs::NR21: {
      static constexpr unsigned dutyShift = 6U;
      static constexpr unsigned dutyMask = 0b11U;
      static constexpr unsigned lengthMask = 0b0011'1111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      PulseChannel& pulse = (address == regs::NR11) ? m_pulse1 : m_pulse2;
      pulse.configuration.duty =
        static_cast<std::uint8_t>((unsignedValue >> dutyShift) & dutyMask);
      pulse.configuration.lengthTimer =
        static_cast<std::uint8_t>(unsignedValue & lengthMask);
      break;
    }

    case regs::NR12:
    case regs::NR22:
    case regs::NR42: {
      EnvelopeConfig* envelope = nullptr;
      bool* enabled = nullptr;
      if (address == regs::NR12) {
        envelope = &m_pulse1.configuration.envelope;
        enabled = &m_pulse1.playback.enabled;
      } else if (address == regs::NR22) {
        envelope = &m_pulse2.configuration.envelope;
        enabled = &m_pulse2.playback.enabled;
      } else {
        envelope = &m_noise.configuration.envelope;
        enabled = &m_noise.playback.enabled;
      }
      writeEnvelope(*envelope, value);
      // "Setting bits 3-7 of this register all to 0 ... turns the DAC off
      // (and thus, the channel as well)".
      if (!isDacEnabled(*envelope)) {
        *enabled = false;
      }
      break;
    }

    case regs::NR13:
    case regs::NR23: {
      static constexpr unsigned periodHighBitsMask = 0b0000'0111'0000'0000U;

      PulseChannel& pulse = (address == regs::NR13) ? m_pulse1 : m_pulse2;
      pulse.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(pulse.configuration.period) &
         periodHighBitsMask) |
        static_cast<unsigned>(value));
      break;
    }

    case regs::NR14:
    case regs::NR24: {
      static constexpr unsigned triggerBit = 0b1000'0000U;
      static constexpr unsigned lengthEnableBit = 0b0100'0000U;
      static constexpr unsigned periodHighMask = 0b111U;
      static constexpr unsigned periodHighShift = 8U;
      static constexpr unsigned periodLowBitsMask = 0x00FFU;

      const auto unsignedValue = static_cast<unsigned>(value);
      PulseChannel& pulse = (address == regs::NR14) ? m_pulse1 : m_pulse2;
      pulse.configuration.isLengthEnabled =
        (unsignedValue & lengthEnableBit) != 0;
      pulse.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(pulse.configuration.period) &
         periodLowBitsMask) |
        ((unsignedValue & periodHighMask) << periodHighShift));
      if ((unsignedValue & triggerBit) != 0) {
        pulse.playback.enabled = isDacEnabled(pulse.configuration.envelope);
      }
      break;
    }

    case regs::NR30: {
      static constexpr unsigned dacEnabledBit = 0b1000'0000U;
      m_wave.configuration.dacEnabled =
        (static_cast<unsigned>(value) & dacEnabledBit) != 0;
      if (!m_wave.configuration.dacEnabled) {
        m_wave.playback.enabled = false;
      }
      break;
    }

    case regs::NR31: {
      m_wave.configuration.lengthTimer = value;
      break;
    }

    case regs::NR32: {
      static constexpr unsigned outputLevelShift = 5U;
      static constexpr unsigned outputLevelMask = 0b11U;
      m_wave.configuration.outputLevel = static_cast<std::uint8_t>(
        (static_cast<unsigned>(value) >> outputLevelShift) & outputLevelMask);
      break;
    }

    case regs::NR33: {
      static constexpr unsigned periodHighBitsMask = 0b0000'0111'0000'0000U;
      m_wave.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_wave.configuration.period) &
         periodHighBitsMask) |
        static_cast<unsigned>(value));
      break;
    }

    case regs::NR34: {
      static constexpr unsigned triggerBit = 0b1000'0000U;
      static constexpr unsigned lengthEnableBit = 0b0100'0000U;
      static constexpr unsigned periodHighMask = 0b111U;
      static constexpr unsigned periodHighShift = 8U;
      static constexpr unsigned periodLowBitsMask = 0x00FFU;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_wave.configuration.isLengthEnabled =
        (unsignedValue & lengthEnableBit) != 0;
      m_wave.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_wave.configuration.period) &
         periodLowBitsMask) |
        ((unsignedValue & periodHighMask) << periodHighShift));
      if ((unsignedValue & triggerBit) != 0) {
        m_wave.playback.enabled = m_wave.configuration.dacEnabled;
      }
      break;
    }

    case regs::NR41: {
      static constexpr unsigned lengthMask = 0b0011'1111U;
      m_noise.configuration.lengthTimer =
        static_cast<std::uint8_t>(static_cast<unsigned>(value) & lengthMask);
      break;
    }

    case regs::NR43: {
      static constexpr unsigned clockShiftShift = 4U;
      static constexpr unsigned clockShiftMask = 0b1111U;
      static constexpr unsigned narrowLfsrBit = 0b0000'1000U;
      static constexpr unsigned clockDividerMask = 0b111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_noise.configuration.clockShift = static_cast<std::uint8_t>(
        (unsignedValue >> clockShiftShift) & clockShiftMask);
      m_noise.configuration.narrowLfsr = (unsignedValue & narrowLfsrBit) != 0;
      m_noise.configuration.clockDivider =
        static_cast<std::uint8_t>(unsignedValue & clockDividerMask);
      break;
    }

    case regs::NR44: {
      static constexpr unsigned triggerBit = 0b1000'0000U;
      static constexpr unsigned lengthEnableBit = 0b0100'0000U;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_noise.configuration.isLengthEnabled =
        (unsignedValue & lengthEnableBit) != 0;
      if ((unsignedValue & triggerBit) != 0) {
        m_noise.playback.enabled = isDacEnabled(m_noise.configuration.envelope);
      }
      break;
    }

    case regs::NR50: {
      static constexpr unsigned leftVolumeShift = 4U;
      static constexpr unsigned volumeMask = 0b111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_leftVolume = static_cast<std::uint8_t>(
        (unsignedValue >> leftVolumeShift) & volumeMask);
      m_rightVolume = static_cast<std::uint8_t>(unsignedValue & volumeMask);
      break;
    }

    case regs::NR52: {
      static constexpr unsigned powerBit = 0b1000'0000U;
      m_powered = (static_cast<unsigned>(value) & powerBit) != 0;
      if (!m_powered) {
        // Mirrors Mmu's own NR10-NR51 byte-range clearing on power-off -
        // keep Apu's channel state and Mmu's raw register bytes consistent
        // with each other.
        m_pulse1 = PulseChannel1{};
        m_pulse2 = PulseChannel{};
        m_wave = WaveChannel{};
        m_noise = NoiseChannel{};
        m_leftVolume = 0;
        m_rightVolume = 0;
      }
      break;
    }

    default:
      break;
  }
}

std::uint8_t
Apu::readRegister(std::uint16_t address) const
{
  if (address == regs::NR52) {
    static constexpr unsigned powerBit = 0b1000'0000U;
    static constexpr unsigned unusedBits = 0b0111'0000U;
    static constexpr unsigned ch1Bit = 0b0000'0001U;
    static constexpr unsigned ch2Bit = 0b0000'0010U;
    static constexpr unsigned ch3Bit = 0b0000'0100U;
    static constexpr unsigned ch4Bit = 0b0000'1000U;

    unsigned result = unusedBits;
    if (m_powered) {
      result |= powerBit;
    }
    if (m_pulse1.playback.enabled) {
      result |= ch1Bit;
    }
    if (m_pulse2.playback.enabled) {
      result |= ch2Bit;
    }
    if (m_wave.playback.enabled) {
      result |= ch3Bit;
    }
    if (m_noise.playback.enabled) {
      result |= ch4Bit;
    }
    return static_cast<std::uint8_t>(result);
  }
  return 0;
}

void
Apu::PulseChannel::runNextTCycle()
{
  if (playback.periodCounter == 0) {
    static constexpr std::uint16_t periodBase = 2048;
    static constexpr std::uint16_t periodMultiplier = 4;
    playback.periodCounter = static_cast<std::uint16_t>(
      periodMultiplier * (periodBase - configuration.period));
    static constexpr std::uint8_t dutyStepCount = 8;
    playback.dutyStep =
      static_cast<std::uint8_t>((playback.dutyStep + 1) % dutyStepCount);
    playback.output = static_cast<std::uint8_t>(
      DUTY_CYCLES.at(configuration.duty).at(playback.dutyStep) *
      playback.volume);
  } else {
    --playback.periodCounter;
  }
}

void
Apu::PulseChannel::clockLength()
{
  // TODO: not implemented yet.
}

void
Apu::PulseChannel::clockEnvelope()
{
  // TODO: not implemented yet.
}

void
Apu::PulseChannel1::clockSweep()
{
  // TODO: not implemented yet.
}

void
Apu::WaveChannel::runNextTCycle()
{
  // TODO: not implemented yet.
}

void
Apu::WaveChannel::clockLength()
{
  // TODO: not implemented yet.
}

void
Apu::NoiseChannel::runNextTCycle()
{
  // TODO: not implemented yet.
}

void
Apu::NoiseChannel::clockLength()
{
  // TODO: not implemented yet.
}

void
Apu::NoiseChannel::clockEnvelope()
{
  // TODO: not implemented yet.
}

}
