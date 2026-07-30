module gbemu;

namespace {

constexpr std::uint32_t CLOCK_RATE_HZ = 4194304;

}

namespace gbemu {

void
Apu::startFrame()
{
  m_buffer.clear();
}

void
Apu::runNextTCycle()
{
  m_sampleAccumulator += static_cast<std::uint32_t>(SAMPLE_RATE);
  if (m_sampleAccumulator < CLOCK_RATE_HZ) {
    return;
  }
  m_sampleAccumulator -= CLOCK_RATE_HZ;

  // Placeholder silence - this only wires up the T-cycle-driven sample
  // timing and per-frame buffer; actual channel synthesis/mixing isn't
  // implemented yet.
  m_buffer.push_back(0);
  m_buffer.push_back(0);
}

}
