export module gbemu:mapper;

import std;
import :serialization;

namespace gbemu {

// A mapper owns everything the cartridge ROM area (0x0000-0x7FFF) and
// external RAM area (0xA000-0xBFFF) resolve to - the raw ROM/RAM bytes,
// plus whatever bank-select/enable registers a given cartridge type
// exposes. Mmu forwards every access in those two ranges here (see
// Mmu::readByte()/writeByte()) via std::visit(MapperVariant) instead of
// branching on cartridge type itself.
//
// Static dispatch via std::variant + std::visit, not a virtual base -
// MapperVariant below is a closed set of every mapper type this library
// implements, known entirely at compile time, so there's no need to pay
// for (or allow) runtime-open polymorphism. The Mapper concept is what
// makes that safe: it gives a compile-time guarantee every alternative
// actually implements the same shape, something a bare
// std::variant<Ts...> alone can't enforce - nothing would otherwise stop
// adding a type that's missing a method until some unrelated call site
// fails to compile.
template<typename T>
concept Mapper = requires(T& mapper,
                          const T& constMapper,
                          std::uint16_t address,
                          std::uint8_t value,
                          SaveStateWriter& writer,
                          SaveStateReader& reader) {
  { constMapper.readRom(address) } -> std::same_as<std::uint8_t>;
  // Register write (bank switching, RAM-enable, etc.) - ROM itself is
  // never writable, so this never actually stores into ROM bytes.
  { mapper.writeRom(address, value) } -> std::same_as<void>;
  { constMapper.readRam(address) } -> std::same_as<std::uint8_t>;
  { mapper.writeRam(address, value) } -> std::same_as<void>;
  // Restores RAM and every register to power-on defaults, as if the
  // cartridge were being run for the very first time - used by
  // GameBoy::reset()'s power-cycle semantics. Battery-backed RAM
  // persistence across sessions is saveState()/loadState()'s job, not
  // this one.
  { mapper.reset() } -> std::same_as<void>;
  // ROM bytes are never part of a save state (same reasoning as Mmu's
  // own m_rom/m_bootRom before this refactor - the caller is expected to
  // loadRom() the same cartridge before deserialize()); only RAM and
  // register state round-trip here.
  { constMapper.serialize(writer) } -> std::same_as<void>;
  { mapper.deserialize(reader) } -> std::same_as<void>;
};

// Cartridge type 0x00 ("ROM ONLY"): no bank switching. Also the fallback
// for any cartridge type this library doesn't yet recognize (carried over
// unchanged from Mmu's pre-refactor cartridgeType check, which already
// treated every non-MBC1 type this way - not a new widening of scope
// here).
class RomOnlyMapper // NOLINT(misc-use-internal-linkage)
{
public:
  RomOnlyMapper() = default;
  explicit RomOnlyMapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);
  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void reset();

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t RAM_SIZE = 0x2000;

  std::vector<std::uint8_t> m_rom;
  // Present unconditionally, matching this library's existing behavior of
  // always backing 0xA000-0xBFFF with 8KB regardless of whether the
  // cartridge header actually declares any RAM - preserved as-is by this
  // refactor rather than newly gated here (see README's own "no external
  // RAM/battery saves yet"; RAM-size/enable gating is future work, not
  // part of this change).
  std::array<std::uint8_t, RAM_SIZE> m_ram{};
};

// Cartridge types 0x01-0x03 (MBC1, MBC1+RAM, MBC1+RAM+BATTERY - the RAM/
// battery distinction isn't yet meaningfully modeled, see RomOnlyMapper's
// own comment on RAM always being present regardless of header
// declaration). ROM banking only: a 5-bit low register (bank 0 reads back
// as 1, real hardware's own well-known quirk) plus a 2-bit high register
// that either extends the switchable-bank number or selects a RAM bank,
// depending on banking mode - see writeRom().
class Mbc1Mapper // NOLINT(misc-use-internal-linkage)
{
public:
  explicit Mbc1Mapper(std::span<const std::uint8_t> rom);

  [[nodiscard]] std::uint8_t readRom(std::uint16_t address) const;
  void writeRom(std::uint16_t address, std::uint8_t value);
  [[nodiscard]] std::uint8_t readRam(std::uint16_t address) const;
  void writeRam(std::uint16_t address, std::uint8_t value);

  void reset();

  void serialize(SaveStateWriter& writer) const;
  void deserialize(SaveStateReader& reader);

private:
  static constexpr std::size_t KB16 = 0x4000;
  static constexpr std::size_t RAM_SIZE = 0x2000;

  // Recomputed from m_romBankLow/m_bankHigh/m_bankingMode on every call
  // rather than cached - it's cheap, and avoids a fourth piece of state
  // that could drift out of sync with the three registers it's purely
  // derived from (m_switchableRomBank, kept as a separate stored field,
  // was exactly that risk before this refactor).
  [[nodiscard]] std::size_t currentRomBank() const;

  std::vector<std::uint8_t> m_rom;
  std::array<std::uint8_t, RAM_SIZE> m_ram{};
  std::uint8_t m_romBankLow{ 1 };
  std::uint8_t m_bankHigh{ 0 };
  bool m_bankingMode{ false };
};

// Constrains std::variant itself to only ever hold types satisfying
// Mapper - MapperVariant below is built through this rather than a bare
// std::variant<RomOnlyMapper, Mbc1Mapper> so a future mapper type missing
// part of the interface fails to compile right here, at the point its
// name is added to the list, instead of wherever std::visit first happens
// to instantiate a call against it.
template<typename... Ts>
  requires(Mapper<Ts> && ...)
using MapperVariantOf = std::variant<Ts...>;

using MapperVariant = MapperVariantOf<RomOnlyMapper, Mbc1Mapper>;

}
