export module gbemu:ppu;

import :mmu;

namespace gbemu {

class Ppu // NOLINT(misc-use-internal-linkage)
{
  public:
    explicit Ppu(Mmu& mmu)
      : m_mmu(mmu)
    {
    }
    void runNextTCycle();
  private:
    std::reference_wrapper<Mmu> m_mmu;
};

};