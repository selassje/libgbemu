export module gbemu:apu;

import std;

namespace gbemu {

// Games/most emulators target 44100 or 48000 Hz; 44100 chosen as the more
// universally-supported default. Exported so a frontend can configure its
// audio device's frequency to match without duplicating the constant.
export std::size_t constexpr SAMPLE_RATE = 44100;

class Apu // NOLINT(misc-use-internal-linkage)
{
public:
  Apu() = default;

  // Clears the previous frame's samples; call once before running a new
  // frame's worth of T-cycles. Deliberately doesn't touch the sample-timing
  // accumulator (see runNextTCycle()) - that needs to stay continuous
  // across frame boundaries to avoid long-term drift.
  void startFrame();

  // divCounter is Mmu's real 16-bit DIV counter (see Mmu::divCounter()) for
  // this same T-cycle - not read back via a stored Mmu& (that would make
  // Mmu and Apu circularly reference each other, since Mmu already holds
  // an Apu& for channel-register write forwarding); GameBoy::runNextFrame()
  // just passes it through each cycle instead.
  void runNextTCycle(std::uint16_t divCounter);

  // Interleaved stereo samples (L, R, L, R, ...) accumulated so far this
  // frame - size() / 2 sample-frames. Normalized float in [-1, 1] (the DAC's
  // own natural output range) rather than a fixed-point type: avoids an
  // artificial extra quantization step between the HPF (a feedback filter,
  // and so precision-sensitive) and this buffer, and SDL3's SDL_AUDIO_F32
  // is just as native a stream format as S16.
  [[nodiscard]] std::span<const float> buffer() const
  {
    return { m_buffer.data(), m_sampleCount };
  }

  // Called by Mmu::writeByte() for every write to a channel register
  // (NR10-NR44) that actually took effect (i.e. not dropped by the
  // APU-powered-off read-only guard) - parses the byte into whichever
  // channel struct's fields it belongs to. Also handles NR52 (power) -
  // Mmu still owns NR50/NR51 and Wave RAM directly.
  void writeRegister(std::uint16_t address, std::uint8_t value);

  // Called by Mmu::readByte() for NR52 - the only register whose
  // CPU-visible value genuinely depends on live Apu state (each channel's
  // real active status) rather than being a fixed positional bitmask Mmu
  // can compute on its own.
  [[nodiscard]] std::uint8_t readRegister(std::uint16_t address) const;

private:
  // At SAMPLE_RATE=44100, one frame's worth of interleaved stereo samples
  // is ~1476-1478 floats (see runNextTCycle()'s accumulator) - comfortable
  // headroom over that without heap-allocating like a std::vector would.
  static constexpr std::size_t BUFFER_CAPACITY = 2048;
  std::array<float, BUFFER_CAPACITY> m_buffer{};
  // How many of m_buffer's slots hold real samples so far this frame - see
  // buffer() and startFrame().
  std::size_t m_sampleCount{ 0 };
  // Bresenham-style fractional accumulator driving sample timing - the
  // Game Boy's clock doesn't divide evenly into any standard sample rate,
  // so a fixed T-cycles-per-sample divisor would drift; this instead emits
  // a sample whenever accumulated cycles cross the clock rate, carrying the
  // remainder forward.
  std::uint32_t m_sampleAccumulator{ 0 };
  // NR52 bit 7. Named distinctly from the channels' own "enabled" (a
  // channel being actively on) - this is the APU as a whole.
  bool m_powered{ false };

  // Frame sequencer (drives length counter/envelope/sweep timing at
  // 512 Hz) - clocked not by a fixed T-cycle divisor but by watching bit 4
  // of DIV (bit 12 of the full 16-bit counter Mmu passes into
  // runNextTCycle()) for a 1->0 transition.
  // TODO: CGB double speed mode watches bit 5 of DIV instead - not handled,
  // since double speed mode itself isn't implemented anywhere in this
  // codebase yet.
  bool m_previousFrameSequencerBit{ false };
  // Increments (wrapping 0-7) on each falling edge detected above - see
  // runNextTCycle() for which channel methods each step dispatches to
  // (length: steps 0/2/4/6, CH1 sweep: 2/6, envelope: 7).
  std::uint8_t m_frameSequencerStep{ 0 };

  // NRx2's volume-envelope layout (initial volume/direction/pace) is
  // identical across CH1, CH2, and CH4 - CH3's wave channel has no
  // envelope at all, just a coarser output-level shift instead (see
  // WaveChannel).
  struct EnvelopeConfig
  {
    // Note: readable, but not updated by the envelope itself as it runs -
    // see PulseChannel::PlaybackState::volume/NoiseChannel::PlaybackState::
    // volume for the live, currently-playing value.
    std::uint8_t initialVolume{ 0 };
    bool increase{ false };
    // Bits 2-0 - ticks at 64 Hz, volume changes every pace-many ticks; 0
    // disables the envelope.
    std::uint8_t pace{ 0 };
  };

  // Shared by both pulse channels - CH1 (below) adds a period sweep on top
  // of this, it isn't itself "a kind of" CH2.
  struct PulseChannel
  {
    // What a register write directly set - never mutated by anything else.
    struct Configuration
    {
      std::uint8_t duty{ 0 };
      // Initial/loaded value - not the live countdown (see PlaybackState).
      std::uint8_t lengthTimer{ 0 };
      EnvelopeConfig envelope;
      // Combined NRx3 (low 8 bits) + NRx4 (high 3 bits) - the configured
      // setting, not the live period counter (see PlaybackState).
      std::uint16_t period{ 0 };
      bool isLengthEnabled{ false };
    } configuration;

    // What's actually happening right now - mutated by runNextTCycle()/the
    // frame sequencer, not directly by register writes.
    struct PlaybackState
    {
      // Whether the channel is currently active - set by triggering
      // (NRx4 bit 7, a one-shot write-only action, not stored as-is),
      // cleared by length expiry, sweep overflow (CH1 only), or the DAC
      // turning off.
      bool enabled{ false };
      // Counts down from 4*(2048-period) to 0 every T-cycle; hitting 0
      // advances dutyStep and reloads from configuration.period's current
      // value (which can change live via NR13/NR14 while playing).
      std::uint16_t periodCounter{ 0 };
      // 0-7, which entry of DUTY_CYCLES[configuration.duty] is playing.
      std::uint8_t dutyStep{ 0 };
      // 0-15, the live, currently-playing volume - reset from
      // configuration.envelope.initialVolume on trigger, then stepped
      // TODO: up/down by the envelope while playing - not implemented.
      // Distinct from initialVolume, which the envelope never modifies.
      std::uint8_t volume{ 0 };
      // 0-15, the channel's current digital amplitude before DAC/mixing:
      // DUTY_CYCLES[configuration.duty][dutyStep] gates volume on/off (the
      // duty waveform is a 1-bit-per-step multiplier, not an amplitude of
      // its own).
      std::uint8_t output{ 0 };
    } playback;

    void runNextTCycle();

    // Frame-sequencer-driven events (see Apu::runNextTCycle()) - steps 0/2/
    // 4/6 (256 Hz) and 7 (64 Hz) respectively.
    // TODO: not implemented yet.
    void clockLength();
    void clockEnvelope();
  };

  // CH1-only. Operates on PulseChannel1::period (inherited) directly -
  // real hardware reads/writes NR13/NR14 in place on each sweep iteration,
  // there's no separate period value of its own here.
  struct PeriodSweep
  {
    // NR10 bits 6-4 - how often sweep iterations happen, in units of
    // 128 Hz ticks. Not re-read by hardware until an iteration completes
    // or the channel retriggers, except that writing 0 here disables
    // iterations instantly.
    std::uint8_t pace{ 0 };
    bool isIncrease{ false };
    // NR10 bits 2-0 - the shift amount in the period-sweep formula
    // (Lt+1 = Lt +/- Lt / 2^shift), not related to the channel's period
    // itself.
    std::uint8_t shift{ 0 };
  };

  struct PulseChannel1 : PulseChannel
  {
    PeriodSweep sweep;

    // CH1-only frame-sequencer event - steps 2/6 (128 Hz).
    // TODO: not implemented yet.
    void clockSweep();
  };

  // CH3 - plays back Wave RAM (see regs::WAVE_RAM_START) instead of a
  // generated duty cycle, so unlike the pulse/noise channels it has no
  // envelope: volume is a single coarse output-level shift, and DAC
  // enable is its own explicit register bit (NR30) rather than something
  // derived from envelope fields the way CH1/CH2/CH4's implicitly is
  // (initial volume 0 + decreasing = DAC off, per NR12/22/42's spec -
  // deliberately not modeled as a redundant stored bit here).
  struct WaveChannel
  {
    struct Configuration
    {
      bool dacEnabled{ false };
      // NR31 - unlike the pulse/noise channels' 6-bit length timers, this
      // is the full 8 bits (channel 3's length counter has a wider range).
      std::uint8_t lengthTimer{ 0 };
      // NR32 bits 6-5: 0 = mute, 1 = 100%, 2 = 50% (samples shifted right
      // once), 3 = 25% (shifted right twice).
      std::uint8_t outputLevel{ 0 };
      // Combined NR33 (low 8 bits) + NR34 (high 3 bits).
      std::uint16_t period{ 0 };
      bool isLengthEnabled{ false };
    } configuration;

    struct PlaybackState
    {
      bool enabled{ false };
      // Counts down from 2*(2048-period) to 0 every T-cycle - twice the
      // pulse channels' rate, per Wave RAM's own frequency formula.
      std::uint16_t periodCounter{ 0 };
      // 0-31, which Wave RAM sample is currently playing.
      std::uint8_t waveRamIndex{ 0 };
      // 0-15, the channel's current digital amplitude before DAC/mixing:
      // the Wave RAM nibble at waveRamIndex, right-shifted per
      // configuration.outputLevel (0/1/2 bits, or forced 0 if muted).
      std::uint8_t output{ 0 };
    } playback;

    void runNextTCycle();

    // Frame-sequencer event - steps 0/2/4/6 (256 Hz). No envelope on this
    // channel (see the class comment above), so no clockEnvelope().
    // TODO: not implemented yet.
    void clockLength();
  };

  // CH4 - white noise via an LFSR instead of a duty cycle or wave table,
  // so it has an envelope (like CH1/CH2) but no period/frequency register
  // of its own; NR43 drives its clocking instead.
  struct NoiseChannel
  {
    struct Configuration
    {
      std::uint8_t lengthTimer{ 0 };
      EnvelopeConfig envelope;
      // NR43 bits 7-4 - see the frequency formula in NR43's own docs.
      std::uint8_t clockShift{ 0 };
      // NR43 bit 3 - false = 15-bit LFSR, true = 7-bit (more
      // regular-sounding output).
      bool narrowLfsr{ false };
      // NR43 bits 2-0 - divider = 0 is treated as 0.5.
      std::uint8_t clockDivider{ 0 };
      bool isLengthEnabled{ false };
    } configuration;

    struct PlaybackState
    {
      bool enabled{ false };
      // The actual running 15/7-bit linear feedback shift register.
      std::uint16_t lfsr{ 0 };
      // Counts down to 0 per NR43's clock shift/divider formula, shifting
      // the LFSR by one bit each time it does.
      std::uint16_t periodCounter{ 0 };
      // 0-15, the live, currently-playing volume - reset from
      // configuration.envelope.initialVolume on trigger, then stepped
      // TODO: up/down by the envelope while playing - not implemented.
      // Distinct from initialVolume, which the envelope never modifies.
      std::uint8_t volume{ 0 };
      // 0-15, the channel's current digital amplitude before DAC/mixing:
      // volume gated by the LFSR's output bit (0 if the shifted-out bit is
      // 0, volume otherwise).
      std::uint8_t output{ 0 };
    } playback;

    void runNextTCycle();

    // Frame-sequencer-driven events (see Apu::runNextTCycle()) - steps 0/2/
    // 4/6 (256 Hz) and 7 (64 Hz) respectively.
    // TODO: not implemented yet.
    void clockLength();
    void clockEnvelope();
  };

  // Shared by the 5 NR12/22/42 (envelope write) and NR14/24/44 (trigger)
  // call sites in writeRegister() - "initial volume 0 + decreasing"
  // (bits 3-7 all zero) is the DAC-off condition for any envelope-having
  // channel.
  [[nodiscard]] static bool isDacEnabled(const EnvelopeConfig& envelope)
  {
    return envelope.initialVolume != 0 || envelope.increase;
  }
  static void writeEnvelope(EnvelopeConfig& envelope, std::uint8_t value);

  PulseChannel1 m_pulse1;
  PulseChannel m_pulse2;
  WaveChannel m_wave;
  NoiseChannel m_noise;
};

};
