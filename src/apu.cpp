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
  // The frame sequencer's step counter is frozen while the APU is
  // powered off (real hardware quirk - see dmg_sound/07-len sweep period
  // sync.gb's "Powering up APU MODs next frame time with 8192": the
  // first post-power-on clock isn't a fixed 8192 T-cycles away, it's
  // however long DIV's bit 12 - which keeps ticking regardless of APU
  // power - takes to naturally transition next). m_previousFrameSequencerBit
  // itself still updates unconditionally below, so that tracking stays
  // accurate the instant power resumes.
  if (m_powered && m_previousFrameSequencerBit && !currentFrameSequencerBit) {
    // Process the CURRENT step, then advance - not the other way around.
    // m_frameSequencerStep starts at 0, and per the frame sequencer's own
    // spec, step 0 is a length-clock step: incrementing before processing
    // would make the very first edge process step 1 (a no-op) instead,
    // permanently shifting every subsequent step by one edge.
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
    static constexpr std::uint8_t frameSequencerStepCount = 8;
    m_frameSequencerStep = static_cast<std::uint8_t>(
      (m_frameSequencerStep + 1) % frameSequencerStepCount);
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

  // Kept as separate statements (not applyHpf(mix())) so the pre-filter
  // mixed output stays inspectable on its own.
  const auto mixed = mix();
  const auto [left, right] = applyHpf(mixed);
  // mix() itself is normalized to [-1, 1] (see its own comment), but
  // applyHpf()'s capacitor filter can still transiently overshoot that
  // range for a sample or two right after a sudden polarity flip (its
  // capacitor hasn't caught up yet) - this clamp is the safety net for
  // that, keeping buffer()'s own documented [-1, 1] contract true.
  m_buffer.at(m_sampleCount++) = std::clamp(left, -1.0F, 1.0F);
  m_buffer.at(m_sampleCount++) = std::clamp(right, -1.0F, 1.0F);
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
  // A channel whose DAC is off contributes nothing at all - not even
  // toDacOutput(0)'s -1.0 bias, since real hardware disconnects it from
  // the mixer entirely. One whose DAC is on but not currently playing
  // (length-expired, untriggered, ...) still contributes that bias,
  // which is exactly what applyHpf() exists to remove.
  const float pulse1Output = isDacEnabled(m_pulse1.configuration.envelope)
                               ? toDacOutput(m_pulse1.playback.output)
                               : 0.0F;
  const float pulse2Output = isDacEnabled(m_pulse2.configuration.envelope)
                               ? toDacOutput(m_pulse2.playback.output)
                               : 0.0F;
  const float waveOutput = m_wave.configuration.dacEnabled
                             ? toDacOutput(m_wave.playback.output)
                             : 0.0F;
  const float noiseOutput = isDacEnabled(m_noise.configuration.envelope)
                              ? toDacOutput(m_noise.playback.output)
                              : 0.0F;

  // NR51 bit N (channel N+1) selects whether that channel is routed to
  // this side - real hardware sums rather than averages these (its analog
  // mixer just saturates past [-1, 1]), but a digital float sample has no
  // such natural ceiling, so this divides by channelCount below to keep
  // buffer()'s own documented [-1, 1] contract true instead.
  static constexpr unsigned ch1Bit = 0b0001U;
  static constexpr unsigned ch2Bit = 0b0010U;
  static constexpr unsigned ch3Bit = 0b0100U;
  static constexpr unsigned ch4Bit = 0b1000U;

  float left = 0.0F;
  if ((m_leftPanning & ch1Bit) != 0) {
    left += pulse1Output;
  }
  if ((m_leftPanning & ch2Bit) != 0) {
    left += pulse2Output;
  }
  if ((m_leftPanning & ch3Bit) != 0) {
    left += waveOutput;
  }
  if ((m_leftPanning & ch4Bit) != 0) {
    left += noiseOutput;
  }

  float right = 0.0F;
  if ((m_rightPanning & ch1Bit) != 0) {
    right += pulse1Output;
  }
  if ((m_rightPanning & ch2Bit) != 0) {
    right += pulse2Output;
  }
  if ((m_rightPanning & ch3Bit) != 0) {
    right += waveOutput;
  }
  if ((m_rightPanning & ch4Bit) != 0) {
    right += noiseOutput;
  }

  static constexpr float channelCount = 4.0F;
  left /= channelCount;
  right /= channelCount;

  static constexpr float maxVolume = 8.0F;
  left *= static_cast<float>(m_leftVolume + 1) / maxVolume;
  right *= static_cast<float>(m_rightVolume + 1) / maxVolume;

  return { left, right };
}

std::pair<float, float>
Apu::applyHpf(std::pair<float, float> input)
{
  const bool anyDacEnabled = isDacEnabled(m_pulse1.configuration.envelope) ||
                             isDacEnabled(m_pulse2.configuration.envelope) ||
                             m_wave.configuration.dacEnabled ||
                             isDacEnabled(m_noise.configuration.envelope);
  if (!anyDacEnabled) {
    // Real hardware's capacitor holds its charge indefinitely while
    // disconnected, rather than decaying toward 0 - freeze it here so a
    // DAC reactivating later doesn't see a spurious jump/pop.
    return { 0.0F, 0.0F };
  }

  // Charge factor for the canonical single-pole "DAC capacitor" model,
  // by convention tuned to 0.999958 per T-cycle (CLOCK_RATE_HZ); since
  // this runs once per output sample (SAMPLE_RATE) instead, the
  // equivalent per-sample coefficient is that value raised to the
  // T-cycles-per-sample power - precomputed offline rather than calling
  // std::pow() every time (both rates are compile-time constants).
  static constexpr float chargeFactor = 0.9960133F;

  const auto [inLeft, inRight] = input;
  const float outLeft = inLeft - m_leftCapacitor;
  m_leftCapacitor = inLeft - (outLeft * chargeFactor);
  const float outRight = inRight - m_rightCapacitor;
  m_rightCapacitor = inRight - (outRight * chargeFactor);

  return { outLeft, outRight };
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
      const bool wasIncrease = m_pulse1.sweep.isIncrease;
      m_pulse1.sweep.pace =
        static_cast<std::uint8_t>((unsignedValue >> paceShift) & paceMask);
      m_pulse1.sweep.isIncrease = (unsignedValue & directionBit) == 0;
      m_pulse1.sweep.shift =
        static_cast<std::uint8_t>(unsignedValue & shiftMask);
      // Real hardware quirk: switching out of negate/subtract mode after
      // it's actually been used to calculate (not just selected) since
      // the last trigger immediately disables the channel - see
      // PulseChannel1::negateModeUsedSinceTrigger's comment.
      if (!wasIncrease && m_pulse1.sweep.isIncrease &&
          m_pulse1.negateModeUsedSinceTrigger) {
        m_pulse1.playback.enabled = false;
      }
      break;
    }

    case regs::NR11:
    case regs::NR21: {
      static constexpr unsigned dutyShift = 6U;
      static constexpr unsigned dutyMask = 0b11U;
      static constexpr unsigned lengthMask = 0b0011'1111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      PulseChannel& pulse = (address == regs::NR11) ? m_pulse1 : m_pulse2;
      // While the APU is powered off, Mmu::writeByte() already merges
      // this write's length bits into the EXISTING stored duty bits
      // before forwarding here (only the length-timer load circuit
      // bypasses the power gate on real hardware, not the duty bits
      // sharing this register) - so parsing duty out of value normally
      // is always correct, powered or not.
      pulse.configuration.duty =
        static_cast<std::uint8_t>((unsignedValue >> dutyShift) & dutyMask);
      pulse.configuration.lengthTimer =
        static_cast<std::uint8_t>(unsignedValue & lengthMask);
      // Real hardware reloads the live length counter immediately on any
      // write to this register, not just at trigger - unlike the
      // trigger-time reload (only when already expired), this always
      // applies.
      static constexpr std::uint16_t maxLengthTicks = 64;
      pulse.playback.remainingLengthTicks = static_cast<std::uint16_t>(
        maxLengthTicks - pulse.configuration.lengthTimer);
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
      const bool wasLengthEnabled = pulse.configuration.isLengthEnabled;
      const bool nowLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
      pulse.configuration.isLengthEnabled = nowLengthEnabled;
      // "Extra length clock" quirk: enabling length (0->1) when the next
      // frame-sequencer edge wouldn't clock it anyway causes an immediate
      // bonus clock - see frameSequencerWontClockLengthNext()'s comment.
      if (!wasLengthEnabled && nowLengthEnabled &&
          frameSequencerWontClockLengthNext()) {
        pulse.clockLength();
      }
      pulse.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(pulse.configuration.period) &
         periodLowBitsMask) |
        ((unsignedValue & periodHighMask) << periodHighShift));
      if ((unsignedValue & triggerBit) != 0) {
        pulse.playback.enabled = isDacEnabled(pulse.configuration.envelope);
        if (pulse.playback.remainingLengthTicks == 0) {
          // Jams to the hardcoded maximum, NOT recomputed from the
          // length-timer register's current value - a real hardware
          // quirk, confirmed by dmg_sound/02-len ctr.gb's "Trigger should
          // treat 0 length as maximum" check.
          static constexpr std::uint16_t maxLengthTicks = 64;
          pulse.playback.remainingLengthTicks = maxLengthTicks;
          // The same extra-clock quirk also applies to this fresh
          // reload, when length is (now) enabled and the phase condition
          // still holds - see dmg_sound/03-trigger.gb's "Triggering that
          // clocks length of 1 should clock twice" check.
          if (nowLengthEnabled && frameSequencerWontClockLengthNext()) {
            pulse.clockLength();
          }
        }
        pulse.playback.volume = pulse.configuration.envelope.initialVolume;
        pulse.playback.envelopeTicksRemaining =
          pulse.configuration.envelope.pace;
        if (address == regs::NR14) {
          m_pulse1.shadowPeriod = m_pulse1.configuration.period;
          m_pulse1.negateModeUsedSinceTrigger = false;
          static constexpr std::uint8_t sweepTimerReloadWhenPaceZero = 8;
          m_pulse1.sweepTimer = (m_pulse1.sweep.pace != 0)
                                  ? m_pulse1.sweep.pace
                                  : sweepTimerReloadWhenPaceZero;
          m_pulse1.sweepEnabled =
            (m_pulse1.sweep.pace != 0) || (m_pulse1.sweep.shift != 0);
          if (m_pulse1.sweep.shift != 0) {
            // Overflow-only check - the result itself isn't used here.
            (void)m_pulse1.calculateSweepFrequency();
          }
        }
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
      // See NR11/NR21's comment: any write reloads the live counter
      // immediately, not just an expired one at trigger.
      static constexpr std::uint16_t maxLengthTicks = 256;
      m_wave.playback.remainingLengthTicks = static_cast<std::uint16_t>(
        maxLengthTicks - m_wave.configuration.lengthTimer);
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
      const bool wasLengthEnabled = m_wave.configuration.isLengthEnabled;
      const bool nowLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
      m_wave.configuration.isLengthEnabled = nowLengthEnabled;
      // See PulseChannel's NR14/NR24 comment on the extra-clock quirk.
      if (!wasLengthEnabled && nowLengthEnabled &&
          frameSequencerWontClockLengthNext()) {
        m_wave.clockLength();
      }
      m_wave.configuration.period = static_cast<std::uint16_t>(
        (static_cast<unsigned>(m_wave.configuration.period) &
         periodLowBitsMask) |
        ((unsignedValue & periodHighMask) << periodHighShift));
      if ((unsignedValue & triggerBit) != 0) {
        m_wave.playback.enabled = m_wave.configuration.dacEnabled;
        if (m_wave.playback.remainingLengthTicks == 0) {
          // See PulseChannel's NR14/NR24 comment: jams to the hardcoded
          // maximum, not recomputed from the length-timer register.
          static constexpr std::uint16_t maxLengthTicks = 256;
          m_wave.playback.remainingLengthTicks = maxLengthTicks;
          if (nowLengthEnabled && frameSequencerWontClockLengthNext()) {
            m_wave.clockLength();
          }
        }
        // Real hardware restarts Wave RAM playback from its first sample
        // on every trigger.
        m_wave.playback.waveRamIndex = 0;
      }
      break;
    }

    case regs::NR41: {
      static constexpr unsigned lengthMask = 0b0011'1111U;
      m_noise.configuration.lengthTimer =
        static_cast<std::uint8_t>(static_cast<unsigned>(value) & lengthMask);
      // See NR11/NR21's comment: any write reloads the live counter
      // immediately, not just an expired one at trigger.
      static constexpr std::uint16_t maxLengthTicks = 64;
      m_noise.playback.remainingLengthTicks = static_cast<std::uint16_t>(
        maxLengthTicks - m_noise.configuration.lengthTimer);
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
      const bool wasLengthEnabled = m_noise.configuration.isLengthEnabled;
      const bool nowLengthEnabled = (unsignedValue & lengthEnableBit) != 0;
      m_noise.configuration.isLengthEnabled = nowLengthEnabled;
      // See PulseChannel's NR14/NR24 comment on the extra-clock quirk.
      if (!wasLengthEnabled && nowLengthEnabled &&
          frameSequencerWontClockLengthNext()) {
        m_noise.clockLength();
      }
      if ((unsignedValue & triggerBit) != 0) {
        m_noise.playback.enabled = isDacEnabled(m_noise.configuration.envelope);
        if (m_noise.playback.remainingLengthTicks == 0) {
          // See PulseChannel's NR14/NR24 comment: jams to the hardcoded
          // maximum, not recomputed from the length-timer register.
          static constexpr std::uint16_t maxLengthTicks = 64;
          m_noise.playback.remainingLengthTicks = maxLengthTicks;
          if (nowLengthEnabled && frameSequencerWontClockLengthNext()) {
            m_noise.clockLength();
          }
        }
        m_noise.playback.volume = m_noise.configuration.envelope.initialVolume;
        m_noise.playback.envelopeTicksRemaining =
          m_noise.configuration.envelope.pace;
        // All 15 bits set - real hardware's power-on/trigger LFSR state.
        static constexpr std::uint16_t lfsrResetValue = 0x7FFF;
        m_noise.playback.lfsr = lfsrResetValue;
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

    case regs::NR51: {
      static constexpr unsigned rightPanningMask = 0b0000'1111U;
      static constexpr unsigned leftPanningShift = 4U;
      static constexpr unsigned leftPanningMask = 0b0000'1111U;

      const auto unsignedValue = static_cast<unsigned>(value);
      m_rightPanning =
        static_cast<std::uint8_t>(unsignedValue & rightPanningMask);
      m_leftPanning = static_cast<std::uint8_t>(
        (unsignedValue >> leftPanningShift) & leftPanningMask);
      break;
    }

    case regs::NR52: {
      static constexpr unsigned powerBit = 0b1000'0000U;
      const bool wasPowered = m_powered;
      m_powered = (static_cast<unsigned>(value) & powerBit) != 0;
      if (!wasPowered && m_powered) {
        // Real hardware resets the frame sequencer's step counter exactly
        // at power-on (not power-off) - runNextTCycle() freezes step
        // processing entirely while powered off (see its own comment), so
        // this is the only reset needed; nothing can have drifted the
        // step while off. m_previousFrameSequencerBit is deliberately
        // left alone: it tracks DIV's real bit state, which keeps
        // changing regardless of APU power.
        m_frameSequencerStep = 0;
      }
      if (!m_powered) {
        // Mirrors Mmu's own NR10-NR51 byte-range clearing on power-off -
        // keep Apu's channel state and Mmu's raw register bytes consistent
        // with each other. Wave RAM is deliberately preserved across this
        // reset (real hardware doesn't clear it on power-off, only
        // NR10-NR51 - see Mmu::writeByte()'s own NR52 handling).
        const auto waveRam = m_wave.configuration.waveRam;
        // Each channel's length counter is preserved through a power
        // cycle on DMG, but reset on CGB (real hardware difference, even
        // in CGB compatibility mode - see dmg_sound/08-len ctr during
        // power.gb's own comment). Either way it stops being clocked
        // while off, via runNextTCycle()'s own power-freeze.
        const auto pulse1LengthTicks = m_pulse1.playback.remainingLengthTicks;
        const auto pulse2LengthTicks = m_pulse2.playback.remainingLengthTicks;
        const auto waveLengthTicks = m_wave.playback.remainingLengthTicks;
        const auto noiseLengthTicks = m_noise.playback.remainingLengthTicks;
        m_pulse1 = PulseChannel1{};
        m_pulse2 = PulseChannel{};
        m_wave = WaveChannel{};
        m_wave.configuration.waveRam = waveRam;
        m_noise = NoiseChannel{};
        if (!m_isCgbHardware) {
          m_pulse1.playback.remainingLengthTicks = pulse1LengthTicks;
          m_pulse2.playback.remainingLengthTicks = pulse2LengthTicks;
          m_wave.playback.remainingLengthTicks = waveLengthTicks;
          m_noise.playback.remainingLengthTicks = noiseLengthTicks;
        }
        m_leftVolume = 0;
        m_rightVolume = 0;
        m_leftPanning = 0;
        m_rightPanning = 0;
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
Apu::writeWaveRam(std::uint16_t address, std::uint8_t value)
{
  m_wave.configuration.waveRam.at(address - regs::WAVE_RAM_START) = value;
}

void
Apu::PulseChannel::runNextTCycle()
{
  if (!playback.enabled) {
    playback.output = 0;
    return;
  }
  if (playback.periodCounter == 0) {
    static constexpr std::uint16_t periodBase = 2048;
    static constexpr std::uint16_t periodMultiplier = 4;
    // The -1 makes the full period exactly periodMultiplier*(periodBase-
    // period) T-cycles: this branch itself (reload+advance) is the Nth
    // tick, so counting down from N-1 here (not N) is what makes N total
    // ticks elapse between one advance and the next.
    playback.periodCounter = static_cast<std::uint16_t>(
      (periodMultiplier * (periodBase - configuration.period)) - 1);
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
  if (!configuration.isLengthEnabled || playback.remainingLengthTicks == 0) {
    return;
  }
  --playback.remainingLengthTicks;
  if (playback.remainingLengthTicks == 0) {
    playback.enabled = false;
  }
}

void
Apu::PulseChannel::clockEnvelope()
{
  if (configuration.envelope.pace == 0) {
    return;
  }
  if (playback.envelopeTicksRemaining > 0) {
    --playback.envelopeTicksRemaining;
  }
  if (playback.envelopeTicksRemaining == 0) {
    playback.envelopeTicksRemaining = configuration.envelope.pace;
    static constexpr std::uint8_t maxVolume = 15;
    if (configuration.envelope.increase) {
      if (playback.volume < maxVolume) {
        ++playback.volume;
      }
    } else if (playback.volume > 0) {
      --playback.volume;
    }
  }
}

void
Apu::PulseChannel1::clockSweep()
{
  if (sweepTimer > 0) {
    --sweepTimer;
  }
  if (sweepTimer != 0) {
    return;
  }
  static constexpr std::uint8_t sweepTimerReloadWhenPaceZero = 8;
  sweepTimer = (sweep.pace != 0) ? sweep.pace : sweepTimerReloadWhenPaceZero;
  if (!sweepEnabled || sweep.pace == 0) {
    return;
  }
  const std::uint16_t newPeriod = calculateSweepFrequency();
  static constexpr std::uint16_t maxPeriod = 2047;
  if (newPeriod <= maxPeriod && sweep.shift != 0) {
    shadowPeriod = newPeriod;
    configuration.period = newPeriod;
    // Quirk: a second overflow-only check, result discarded - real
    // hardware performs this recalculation even though nothing further
    // gets written back.
    (void)calculateSweepFrequency();
  }
}

std::uint16_t
Apu::PulseChannel1::calculateSweepFrequency()
{
  const auto delta = static_cast<std::uint16_t>(shadowPeriod >> sweep.shift);
  const auto newPeriod = static_cast<std::uint16_t>(
    sweep.isIncrease ? shadowPeriod + delta : shadowPeriod - delta);
  if (!sweep.isIncrease) {
    negateModeUsedSinceTrigger = true;
  }
  static constexpr std::uint16_t maxPeriod = 2047;
  if (newPeriod > maxPeriod) {
    playback.enabled = false;
  }
  return newPeriod;
}

void
Apu::WaveChannel::runNextTCycle()
{
  if (!playback.enabled) {
    playback.output = 0;
    return;
  }
  if (playback.periodCounter == 0) {
    static constexpr std::uint16_t periodBase = 2048;
    static constexpr std::uint16_t periodMultiplier = 2;
    // See PulseChannel::runNextTCycle()'s comment on the -1: this branch
    // is itself the Nth tick of the period, so reloading to N-1 (not N)
    // is what makes exactly N ticks elapse per period.
    playback.periodCounter = static_cast<std::uint16_t>(
      (periodMultiplier * (periodBase - configuration.period)) - 1);

    static constexpr std::uint8_t sampleCount = 32;
    playback.waveRamIndex =
      static_cast<std::uint8_t>((playback.waveRamIndex + 1) % sampleCount);

    // Each byte holds two 4-bit samples - high nibble (even index) played
    // before low nibble (odd index).
    const std::uint8_t sampleByte =
      configuration.waveRam.at(playback.waveRamIndex / 2);
    static constexpr unsigned nibbleShift = 4U;
    static constexpr unsigned nibbleMask = 0x0FU;
    const unsigned nibble =
      (playback.waveRamIndex % 2 == 0)
        ? (static_cast<unsigned>(sampleByte) >> nibbleShift)
        : (static_cast<unsigned>(sampleByte) & nibbleMask);

    // configuration.outputLevel (0-3) indexes directly: 0 = mute (shifting
    // a 4-bit nibble right by 4 always yields 0), 1 = 100%, 2 = 50%,
    // 3 = 25%.
    static constexpr std::array<unsigned, 4> outputShift = { 4, 0, 1, 2 };
    playback.output = static_cast<std::uint8_t>(
      nibble >> outputShift.at(configuration.outputLevel));
  } else {
    --playback.periodCounter;
  }
}

void
Apu::WaveChannel::clockLength()
{
  if (!configuration.isLengthEnabled || playback.remainingLengthTicks == 0) {
    return;
  }
  --playback.remainingLengthTicks;
  if (playback.remainingLengthTicks == 0) {
    playback.enabled = false;
  }
}

void
Apu::NoiseChannel::runNextTCycle()
{
  if (!playback.enabled) {
    playback.output = 0;
    return;
  }
  if (playback.periodCounter == 0) {
    // NR43 bits 2-0 look up a base divisor here (0 meaning 8, i.e. 0.5x
    // the divisor-1 rate); bits 7-4 (clockShift) then left-shift that
    // further - see the class comment on NR43's own frequency formula.
    static constexpr std::array<std::uint16_t, 8> divisorTable = { 8,  16, 32,
                                                                   48, 64, 80,
                                                                   96, 112 };
    // See PulseChannel::runNextTCycle()'s comment on the -1: this branch
    // is itself the Nth tick of the period, so reloading to N-1 (not N)
    // is what makes exactly N ticks elapse per period.
    playback.periodCounter =
      static_cast<std::uint16_t>((divisorTable.at(configuration.clockDivider)
                                  << configuration.clockShift) -
                                 1);

    // LFSR shift: XOR the low two bits, shift everything right by one, and
    // feed the XOR result into what's now the top (bit 14) of the 15-bit
    // register - and also into bit 6 in narrow/7-bit mode, producing a
    // much shorter, more tonal repeat cycle. Kept in an unsigned local
    // throughout (rather than uint16_t's own bitwise ops, which promote to
    // signed int) to avoid hicpp-signed-bitwise.
    auto lfsrValue = static_cast<unsigned>(playback.lfsr);
    const unsigned xorResult = (lfsrValue ^ (lfsrValue >> 1U)) & 0b1U;
    lfsrValue >>= 1U;
    static constexpr unsigned highBitShift = 14U;
    lfsrValue |= xorResult << highBitShift;
    if (configuration.narrowLfsr) {
      static constexpr unsigned narrowBitShift = 6U;
      static constexpr unsigned narrowBitMask = 1U << narrowBitShift;
      lfsrValue = (lfsrValue & ~narrowBitMask) | (xorResult << narrowBitShift);
    }
    playback.lfsr = static_cast<std::uint16_t>(lfsrValue);
    // Output is bit 0 of the LFSR, inverted.
    playback.output = static_cast<std::uint8_t>(
      (~lfsrValue & 0b1U) * static_cast<unsigned>(playback.volume));
  } else {
    --playback.periodCounter;
  }
}

void
Apu::NoiseChannel::clockLength()
{
  if (!configuration.isLengthEnabled || playback.remainingLengthTicks == 0) {
    return;
  }
  --playback.remainingLengthTicks;
  if (playback.remainingLengthTicks == 0) {
    playback.enabled = false;
  }
}

void
Apu::NoiseChannel::clockEnvelope()
{
  if (configuration.envelope.pace == 0) {
    return;
  }
  if (playback.envelopeTicksRemaining > 0) {
    --playback.envelopeTicksRemaining;
  }
  if (playback.envelopeTicksRemaining == 0) {
    playback.envelopeTicksRemaining = configuration.envelope.pace;
    static constexpr std::uint8_t maxVolume = 15;
    if (configuration.envelope.increase) {
      if (playback.volume < maxVolume) {
        ++playback.volume;
      }
    } else if (playback.volume > 0) {
      --playback.volume;
    }
  }
}

}
