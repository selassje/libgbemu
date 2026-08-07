export module gbemu:serialization;

import std;

export namespace gbemu {

// Byte-oriented, fixed little-endian save-state encoding - shared by every
// component's own serialize()/deserialize() (Cpu, Mmu, Ppu, Apu) so the
// format doesn't depend on host endianness or struct layout/padding, and so
// a save file stays portable between the native and Emscripten/wasm builds.
// Multi-byte values are written/read one byte at a time via explicit shifts
// rather than a reinterpret_cast/memcpy of the host value, so the encoding
// is the same regardless of the host's own endianness (every current build
// target happens to be little-endian already, but nothing here relies on
// that).
//
// Each component's serialize()/deserialize() calls these one field at a
// time, by hand, in a fixed order - the only option available in C++23.
// C++26 reflection (once it's available in the toolchains this project
// targets) would let that per-field boilerplate be generated instead of
// hand-maintained, so adding/removing/reordering a member wouldn't require
// touching that component's serialize()/deserialize() at all. Worth
// revisiting then; SaveStateWriter/SaveStateReader themselves (the actual
// byte encoding) would very likely still be needed underneath, reflection
// would just replace the hand-written call sites that invoke them.
class SaveStateWriter
{
public:
  void writeU8(std::uint8_t value);
  void writeU16(std::uint16_t value);
  void writeU32(std::uint32_t value);
  // Always 8 bytes on the wire regardless of the host's own size_t width
  // (4 bytes on wasm32, typically 8 elsewhere) - used for every
  // std::size_t field (bank indices, cumulative cycle counters, ...) so a
  // save file stays portable between native and Emscripten/wasm builds,
  // and so long-running cumulative counters (e.g. Cpu's own T-cycle
  // totals) can't overflow a narrower encoding the way they realistically
  // could in a multi-hour session.
  void writeU64(std::uint64_t value);
  // Convenience wrapper around writeU64() for std::size_t fields (bank
  // indices, cumulative cycle counters, ...) - always encodes as 8 bytes
  // on the wire regardless of the host's own size_t width (4 bytes on
  // wasm32, typically 8 elsewhere), the same portability/overflow
  // reasoning as writeU64() itself. Exists as its own method (rather than
  // callers just doing writeU64(value) themselves) so the size_t -> u64
  // conversion happens exactly once, in a way that's genuinely narrowing
  // only on a 32-bit size_t target - a plain static_cast at every call
  // site would instead be a real cast on some build targets and a
  // same-width no-op cast (i.e. flagged useless, see -Wuseless-cast) on
  // others, which can't both be satisfied by one unconditional cast
  // written directly at the call site.
  void writeSize(std::size_t value);
  void writeBool(bool value);
  // Bit-for-bit via std::bit_cast, not a rounded/reformatted value - every
  // build target's `float` is already IEEE-754 binary32, so this is exact
  // and portable the same way the integer writers above are, without
  // needing a separate floating-point encoding.
  void writeFloat(float value);
  void writeBytes(std::span<const std::uint8_t> bytes);

  [[nodiscard]] const std::vector<std::uint8_t>& bytes() const;

private:
  std::vector<std::uint8_t> m_bytes;
};

// Reads back a buffer written by SaveStateWriter, in the same field order
// the writer was called in - callers are responsible for reading fields in
// that same order, the same way SaveStateWriter's own callers are
// responsible for writing them in a fixed order in the first place.
//
// Out-of-bounds reads (a truncated or corrupt buffer) throw
// std::out_of_range, the same failure mode Mmu's own std::array::at() calls
// already use elsewhere in this codebase for invalid access - callers
// working from untrusted external data (e.g. a save file loaded from disk)
// are expected to catch that at their own boundary, the same way
// GameBoy::loadRom() validates its own untrusted input up front.
class SaveStateReader
{
public:
  explicit SaveStateReader(std::span<const std::uint8_t> data);

  std::uint8_t readU8();
  std::uint16_t readU16();
  std::uint32_t readU32();
  std::uint64_t readU64();
  // See SaveStateWriter::writeSize() - reads back what it wrote.
  std::size_t readSize();
  bool readBool();
  float readFloat();
  void readBytes(std::span<std::uint8_t> out);

private:
  void checkAvailable(std::size_t count) const;

  std::span<const std::uint8_t> m_data;
  std::size_t m_offset{ 0 };
};

}
