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

private:
  std::vector<std::int16_t> m_buffer;
  // Bresenham-style fractional accumulator driving sample timing - the
  // Game Boy's clock doesn't divide evenly into any standard sample rate,
  // so a fixed T-cycles-per-sample divisor would drift; this instead emits
  // a sample whenever accumulated cycles cross the clock rate, carrying the
  // remainder forward.
  std::uint32_t m_sampleAccumulator{ 0 };
};

}
