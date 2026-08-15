export module gbemu:mappers;

import std;
import :serialization;

namespace gbemu {

class Mapper // NOLINT(misc-use-internal-linkage)
{
public:
  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);
  void runNextTCycle()
  { // not every mapper needs to implement this
  }

protected:
  static constexpr std::size_t KB16 = 0x4000;

  Mapper() = default;
  explicit Mapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::size_t romSize() const { return m_rom.size(); }
  [[nodiscard]] std::uint8_t romByte(std::size_t index) const
  {
    return m_rom.at(index);
  }

  // Real hardware only ever exposes as many *distinct* RAM banks as the
  // cartridge's own SRAM chip actually has, declared in its header at
  // 0x0149 - a cartridge with fewer physical banks aliases a bank-select
  // write back onto the bank(s) that do exist (mod ramBankCount()).
  [[nodiscard]] std::size_t ramBankCount() const
  {
    constexpr std::uint16_t ramSizeHeaderAddress = 0x0149;
    if (ramSizeHeaderAddress >= romSize()) {
      return 1;
    }
    switch (romByte(ramSizeHeaderAddress)) {
      case 0x02:
        return 1; // 8 KB
      case 0x03:
        return 4; // 32 KB
      case 0x04:
        return 16; // 128 KB
      case 0x05:
        return 8; // 64 KB
      default:
        return 1;
    }
  }

  // Computed entirely in std::size_t, only ever subtracting EXT_RAM_START
  // from the original address (never adding bank*RAM_BANK_SIZE to a
  // uint16_t first) - address + bank*RAM_BANK_SIZE can overflow
  // std::uint16_t for banks reachable on real MBC1 cartridges (e.g. bank
  // 3), which would wrap outside RAM_SIZE if truncated before the bounds
  // check.
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  [[nodiscard]] std::uint8_t readRam(std::uint16_t address,
                                     std::size_t bank) const;
  // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
  void writeRam(std::uint16_t address, std::size_t bank, std::uint8_t value);

  void serializeRam(SaveStateWriter& writer) const { writer.writeBytes(m_ram); }
  void deserializeRam(SaveStateReader& reader) { reader.readBytes(m_ram); }

  static constexpr std::size_t RAM_BANK_SIZE = 0x2000;
  static constexpr std::size_t RAM_SIZE = RAM_BANK_SIZE * 8;

private:
  std::vector<std::uint8_t> m_rom;
  std::array<std::uint8_t, RAM_SIZE> m_ram{};
};

template<typename T>
concept MapperLike = requires(T& mapper,
                              const T& constMapper,
                              std::uint16_t address,
                              std::uint8_t value,
                              SaveStateWriter& writer,
                              SaveStateReader& reader) {
  { constMapper.readRom(address) } -> std::same_as<std::uint8_t>;
  // ROM itself is never writable - this is a register write (bank
  // switching, RAM-enable, etc.), never a store into ROM bytes.
  { mapper.writeRom(address, value) } -> std::same_as<void>;
  { constMapper.readRam(address) } -> std::same_as<std::uint8_t>;
  { mapper.writeRam(address, value) } -> std::same_as<void>;
  { constMapper.serialize(writer) } -> std::same_as<void>;
  { mapper.deserialize(reader) } -> std::same_as<void>;

  { mapper.runNextTCycle() } -> std::same_as<void>;
};

// Cartridge type 0x00 ("ROM ONLY"): no bank switching.
class RomOnlyMapper // NOLINT(misc-use-internal-linkage)
  : private Mapper
{
public:
  using Mapper::readRam;
  using Mapper::runNextTCycle;
  using Mapper::writeRam;

  RomOnlyMapper() = default;
  explicit RomOnlyMapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);
};

// Cartridge types 0x01-0x03 (MBC1, MBC1+RAM, MBC1+RAM+BATTERY). ROM and
// RAM banking both: a 5-bit low register (bank 0 reads back as 1, real
// hardware's own well-known quirk) plus a 2-bit high register that either
// extends the switchable ROM bank number or selects a RAM bank, depending
// on banking mode. RAM reads/writes are also gated by the RAM-enable
// register - disabled RAM reads back as 0xFF and ignores writes, matching
// real hardware's open-bus behavior for a disconnected SRAM chip.
class Mbc1Mapper // NOLINT(misc-use-internal-linkage)
  : private Mapper
{
public:
  using Mapper::runNextTCycle;

  explicit Mbc1Mapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);

  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  [[nodiscard]] std::size_t currentRomBank() const;

  std::uint8_t m_romBankLow{ 1 };
  std::uint8_t m_bankHigh{ 0 };
  bool m_bankingMode{ false };
  bool m_ramEnabled{ false };
};

class Mbc2Mapper // NOLINT(misc-use-internal-linkage)
  : private Mapper
{
public:
  using Mapper::runNextTCycle;

  explicit Mbc2Mapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);

  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  std::uint8_t m_romBank{ 1 };
  bool m_ramEnabled{ false };
};

class Mbc3Mapper // NOLINT(misc-use-internal-linkage)
  : private Mapper
{
public:
  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void runNextTCycle();

  explicit Mbc3Mapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  [[nodiscard]] std::size_t currentRomBank() const;

  // Bank 0 is always mapped at the fixed 0x0000-0x3FFF window, so real
  // hardware never lets 0 mean anything in the switchable 0x4000-0x7FFF
  // one either.
  std::uint8_t m_romBank{ 1 };
  std::uint8_t m_ramBank{};
  bool m_ramAndTimerEnabled{ false };
  std::optional<std::size_t> m_selectedRtc{ std::nullopt };
  std::array<std::uint8_t, 5> m_rtcRegisters{};
  std::uint8_t m_lastLatchValue{ 0xFF };

  struct RealTimeClock
  {
    std::uint8_t seconds{ 0 };
    std::uint8_t minutes{ 0 };
    std::uint8_t hours{ 0 };
    std::uint16_t days{ 0 };
    bool dayCarry{ false };
    bool halt{ false };
    std::uint64_t tCycles{ 0 };
    void runNextTCycle();
  };

  RealTimeClock m_rtc{};
};
class Mbc5Mapper // NOLINT(misc-use-internal-linkage)
  : private Mapper
{
public:
  using Mapper::runNextTCycle;

  Mbc5Mapper(std::span<const std::uint8_t> rom, bool rumblerEnabled);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);

  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  [[nodiscard]] std::size_t currentRomBank() const;

  // Real MBC5 hardware presents ROM bank 1 at 0x4000-0x7FFF from power-on -
  // well-behaved ROMs rely on this and jump straight into that region
  // before ever writing the bank-select registers themselves.
  std::uint8_t m_romBankLow{ 1 };
  std::uint8_t m_romBankHigh{ 0 };
  bool m_rumblerEnabled{ false };
  bool m_ramEnabled{ false };
  std::uint8_t m_ramBank{ 0 };
};

template<typename... Ts>
  requires(MapperLike<Ts> && ...)
using MapperVariantOf = std::variant<Ts...>;

using MapperVariant = MapperVariantOf<RomOnlyMapper,
                                      Mbc1Mapper,
                                      Mbc2Mapper,
                                      Mbc3Mapper,
                                      Mbc5Mapper>;
}
