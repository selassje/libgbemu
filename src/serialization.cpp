module gbemu;

namespace {

// Only actually casts when narrowing is real for this build's To/From
// widths (e.g. std::uint64_t -> std::size_t on wasm32) - sizeof(To) <
// sizeof(From) depends on the template parameters, so the untaken branch
// is genuinely discarded (never instantiated), not merely dead code, the
// same way a template's discarded if-constexpr branch is never checked
// for a type that doesn't apply to it. That's what lets this compile
// cleanly on every target: a plain static_cast written directly at the
// call site would instead be a real, needed cast on some build targets
// (wasm32) and a same-width no-op cast on others (flagged
// -Wuseless-cast, GCC-only) - both can't be satisfied by one
// unconditional cast.
template<typename To, typename From>
constexpr To
narrowingCast(From value)
{
  if constexpr (sizeof(To) < sizeof(From)) {
    return static_cast<To>(value);
  } else {
    return value;
  }
}

}

namespace gbemu {

void
SaveStateWriter::writeU8(std::uint8_t value)
{
  m_bytes.push_back(value);
}

void
SaveStateWriter::writeU16(std::uint16_t value)
{
  writeU8(static_cast<std::uint8_t>(value & 0xFFU));
  writeU8(
    static_cast<std::uint8_t>((static_cast<unsigned>(value) >> 8U) & 0xFFU));
}

void
SaveStateWriter::writeU32(std::uint32_t value)
{
  writeU8(static_cast<std::uint8_t>(value & 0xFFU));
  writeU8(static_cast<std::uint8_t>((value >> 8U) & 0xFFU));
  writeU8(static_cast<std::uint8_t>((value >> 16U) & 0xFFU));
  writeU8(static_cast<std::uint8_t>((value >> 24U) & 0xFFU));
}

void
SaveStateWriter::writeU64(std::uint64_t value)
{
  writeU32(static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
  writeU32(static_cast<std::uint32_t>((value >> 32U) & 0xFFFFFFFFULL));
}

void
SaveStateWriter::writeSize(std::size_t value)
{
  writeU64(narrowingCast<std::uint64_t>(value));
}

void
SaveStateWriter::writeBool(bool value)
{
  writeU8(value ? 1U : 0U);
}

void
SaveStateWriter::writeFloat(float value)
{
  writeU32(std::bit_cast<std::uint32_t>(value));
}

void
SaveStateWriter::writeBytes(std::span<const std::uint8_t> bytes)
{
  m_bytes.insert(m_bytes.end(), bytes.begin(), bytes.end());
}

const std::vector<std::uint8_t>&
SaveStateWriter::bytes() const
{
  return m_bytes;
}

SaveStateReader::SaveStateReader(std::span<const std::uint8_t> data)
  : m_data(data)
{
}

void
SaveStateReader::checkAvailable(std::size_t count) const
{
  if (count > m_data.size() - m_offset) {
    // MSVC STL's std::out_of_range base-class chain isn't visible to
    // clang-tidy through `import std;`'s module boundary; not
    // reproducible on libc++ (dev_ninja_clang_tidy_linux builds this file
    // clean) - see Ppu::Fifo::push()'s identical comment.
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::out_of_range("SaveStateReader: not enough data remaining");
  }
}

std::uint8_t
SaveStateReader::readU8()
{
  checkAvailable(1);
  // Bounds already verified by checkAvailable() above.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  return m_data[m_offset++];
}

std::uint16_t
SaveStateReader::readU16()
{
  const std::uint16_t low = readU8();
  const std::uint16_t high = readU8();
  return static_cast<std::uint16_t>(low | (static_cast<unsigned>(high) << 8U));
}

std::uint32_t
SaveStateReader::readU32()
{
  const std::uint32_t byte0 = readU8();
  const std::uint32_t byte1 = readU8();
  const std::uint32_t byte2 = readU8();
  const std::uint32_t byte3 = readU8();
  return byte0 | (byte1 << 8U) | (byte2 << 16U) | (byte3 << 24U);
}

std::uint64_t
SaveStateReader::readU64()
{
  const std::uint64_t low = readU32();
  const std::uint64_t high = readU32();
  return low | (high << 32U);
}

std::size_t
SaveStateReader::readSize()
{
  return narrowingCast<std::size_t>(readU64());
}

bool
SaveStateReader::readBool()
{
  return readU8() != 0;
}

float
SaveStateReader::readFloat()
{
  return std::bit_cast<float>(readU32());
}

void
SaveStateReader::readBytes(std::span<std::uint8_t> out)
{
  checkAvailable(out.size());
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  std::copy_n(m_data.begin() + static_cast<std::ptrdiff_t>(m_offset),
              out.size(),
              out.begin());
  m_offset += out.size();
}

}
