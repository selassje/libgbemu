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

  void runNextTCycle();

  // Interleaved stereo samples (L, R, L, R, ...) accumulated so far this
  // frame - size() / 2 sample-frames.
  [[nodiscard]] const std::vector<std::int16_t>& buffer() const
  {
    return m_buffer;
  }

  // Called by Mmu::writeByte() for every write to a channel register
  // (NR10-NR44) that actually took effect (i.e. not dropped by the
  // APU-powered-off read-only guard) - parses the byte into whichever
  // channel struct's fields it belongs to. Global registers (NR50-NR52,
  // Wave RAM) aren't included; Mmu still owns those directly.
  void writeRegister(std::uint16_t address, std::uint8_t value);

private:
  std::vector<std::int16_t> m_buffer;
  // Bresenham-style fractional accumulator driving sample timing - the
  // Game Boy's clock doesn't divide evenly into any standard sample rate,
  // so a fixed T-cycles-per-sample divisor would drift; this instead emits
  // a sample whenever accumulated cycles cross the clock rate, carrying the
  // remainder forward.
  std::uint32_t m_sampleAccumulator{ 0 };

  // NRx2's volume-envelope layout (initial volume/direction/pace) is
  // identical across CH1, CH2, and CH4 - CH3's wave channel has no
  // envelope at all, just a coarser output-level shift instead (see
  // WaveChannel).
  struct Envelope
  {
    // Note: readable, but not updated by the envelope itself as it runs -
    // that live/current volume isn't tracked here yet (see class-level
    // comment on what's still missing).
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
    std::uint8_t duty{ 0 };
    std::uint8_t lengthTimer{ 0 };
    Envelope envelope;
    // Combined NRx3 (low 8 bits) + NRx4 (high 3 bits).
    std::uint16_t period{ 0 };
    // Whether the channel is currently active - set by triggering
    // (NRx4 bit 7, a one-shot write-only action, not stored as-is), cleared
    // by length expiry, sweep overflow (CH1 only), or the DAC turning off.
    bool enabled{ false };
    bool isLengthEnabled{ false };
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
    bool dacEnabled{ false };
    // NR31 - unlike the pulse/noise channels' 6-bit length timers, this is
    // the full 8 bits (channel 3's length counter has a wider range).
    std::uint8_t lengthTimer{ 0 };
    // NR32 bits 6-5: 0 = mute, 1 = 100%, 2 = 50% (samples shifted right
    // once), 3 = 25% (shifted right twice).
    std::uint8_t outputLevel{ 0 };
    // Combined NR33 (low 8 bits) + NR34 (high 3 bits).
    std::uint16_t period{ 0 };
    bool enabled{ false };
    bool isLengthEnabled{ false };
  };

  // CH4 - white noise via an LFSR instead of a duty cycle or wave table,
  // so it has an envelope (like CH1/CH2) but no period/frequency register
  // of its own; NR43 drives its clocking instead.
  struct NoiseChannel
  {
    std::uint8_t lengthTimer{ 0 };
    Envelope envelope;
    // NR43 bits 7-4 - see the frequency formula in NR43's own docs.
    std::uint8_t clockShift{ 0 };
    // NR43 bit 3 - false = 15-bit LFSR, true = 7-bit (more regular-sounding
    // output).
    bool narrowLfsr{ false };
    // NR43 bits 2-0 - divider = 0 is treated as 0.5.
    std::uint8_t clockDivider{ 0 };
    bool enabled{ false };
    bool isLengthEnabled{ false };
  };

  // Shared by the 5 NR12/22/42 (envelope write) and NR14/24/44 (trigger)
  // call sites in writeRegister() - "initial volume 0 + decreasing"
  // (bits 3-7 all zero) is the DAC-off condition for any envelope-having
  // channel.
  [[nodiscard]] static bool isDacEnabled(const Envelope& envelope)
  {
    return envelope.initialVolume != 0 || envelope.increase;
  }
  static void writeEnvelope(Envelope& envelope, std::uint8_t value);

  PulseChannel1 m_pulse1;
  PulseChannel m_pulse2;
  WaveChannel m_wave;
  NoiseChannel m_noise;
};

};
