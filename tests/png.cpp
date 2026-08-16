module png;

import std;

namespace {

// --- Encoding (writePixelsAsPng) -------------------------------------

std::uint32_t
crc32(std::span<const std::uint8_t> data)
{
  static constexpr auto crcTable = [] {
    std::array<std::uint32_t, 256> table{};
    for (std::uint32_t n = 0; n < 256; ++n) {
      std::uint32_t c = n;
      for (int k = 0; k < 8; ++k) {
        c = (c & 1U) != 0 ? (0xEDB88320U ^ (c >> 1U)) : (c >> 1U);
      }
      table.at(n) = c;
    }
    return table;
  }();
  std::uint32_t crc = 0xFFFFFFFFU;
  for (const auto byte : data) {
    crc = crcTable.at((crc ^ byte) & 0xFFU) ^ (crc >> 8U);
  }
  return crc ^ 0xFFFFFFFFU;
}

std::uint32_t
adler32(std::span<const std::uint8_t> data)
{
  constexpr std::uint32_t modAdler = 65521;
  std::uint32_t a = 1;
  std::uint32_t b = 0;
  for (const auto byte : data) {
    a = (a + byte) % modAdler;
    b = (b + a) % modAdler;
  }
  return (b << 16U) | a;
}

void
appendBigEndianU32(std::vector<std::uint8_t>& out, std::uint32_t value)
{
  out.push_back(static_cast<std::uint8_t>(value >> 24U));
  out.push_back(static_cast<std::uint8_t>(value >> 16U));
  out.push_back(static_cast<std::uint8_t>(value >> 8U));
  out.push_back(static_cast<std::uint8_t>(value));
}

void
appendPngChunk(std::vector<std::uint8_t>& out,
               std::string_view type,
               std::span<const std::uint8_t> data)
{
  appendBigEndianU32(out, static_cast<std::uint32_t>(data.size()));
  const auto chunkStart = out.size();
  out.insert(out.end(), type.begin(), type.end());
  out.insert(out.end(), data.begin(), data.end());
  const auto crc = crc32(std::span(out).subspan(chunkStart));
  appendBigEndianU32(out, crc);
}

// --- Decoding (readPngAsRgb/pixelsMatchPng) ---------------------------

constexpr int MAX_CODE_BITS = 15;

// Reads DEFLATE's bitstream convention: bits packed LSB-first within each
// byte, bytes consumed in order.
class BitReader
{
public:
  explicit BitReader(std::span<const std::uint8_t> data)
    : m_data(data)
  {
  }

  int bit()
  {
    if (m_bitPos == 0) {
      if (m_bytePos >= m_data.size()) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: unexpected end of DEFLATE "
                                 "stream");
      }
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      m_current = m_data[m_bytePos++];
    }
    const int value = static_cast<int>(
      (static_cast<unsigned>(m_current) >> static_cast<unsigned>(m_bitPos)) &
      1U);
    m_bitPos = (m_bitPos + 1) % 8;
    return value;
  }

  std::uint32_t bits(int count)
  {
    std::uint32_t value = 0;
    for (int i = 0; i < count; ++i) {
      value |= static_cast<std::uint32_t>(bit()) << static_cast<unsigned>(i);
    }
    return value;
  }

  void alignToByte() { m_bitPos = 0; }

  // Only valid right after alignToByte() - a mid-byte call here would
  // silently skip whatever bits of the current byte hadn't been consumed
  // yet.
  std::uint8_t byte()
  {
    if (m_bytePos >= m_data.size()) {
      // NOLINTNEXTLINE(hicpp-exception-baseclass)
      throw std::runtime_error("readPngAsRgb: unexpected end of DEFLATE "
                               "stream");
    }
    // Bounds already verified by the size check above.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    return m_data[m_bytePos++];
  }

private:
  std::span<const std::uint8_t> m_data;
  std::size_t m_bytePos{ 0 };
  int m_bitPos{ 0 };
  std::uint8_t m_current{ 0 };
};

// Canonical Huffman decode table, built the same way puff.c (Mark Adler's
// reference minimal inflate implementation) does: counts[len] = how many
// symbols have that code length, symbols[] = the symbols themselves sorted
// first by code length then by symbol value - together enough to decode
// one bit at a time without ever materializing the actual code values.
struct HuffmanTable
{
  std::array<std::int16_t, MAX_CODE_BITS + 1> counts{};
  std::vector<std::int16_t> symbols;
};

void
construct(HuffmanTable& table, std::span<const std::uint8_t> lengths)
{
  table.counts.fill(0);
  for (const auto len : lengths) {
    ++table.counts.at(len);
  }
  table.counts.at(0) = 0; // length 0 means "unused symbol", not a real code

  std::array<int, MAX_CODE_BITS + 1> offsets{};
  for (std::size_t len = 1; len < static_cast<std::size_t>(MAX_CODE_BITS);
       ++len) {
    offsets.at(len + 1) = offsets.at(len) + table.counts.at(len);
  }

  table.symbols.assign(lengths.size(), 0);
  for (std::size_t symbol = 0; symbol < lengths.size(); ++symbol) {
    // Loop bound is lengths.size() itself.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
    const auto len = lengths[symbol];
    if (len != 0) {
      table.symbols.at(static_cast<std::size_t>(offsets.at(len)++)) =
        static_cast<std::int16_t>(symbol);
    }
  }
}

int
decodeSymbol(BitReader& reader, const HuffmanTable& table)
{
  unsigned code = 0;
  unsigned first = 0;
  unsigned index = 0;
  for (int len = 1; len <= MAX_CODE_BITS; ++len) {
    code |= static_cast<unsigned>(reader.bit());
    const auto count =
      static_cast<unsigned>(table.counts.at(static_cast<std::size_t>(len)));
    if (code - first < count) {
      return table.symbols.at(static_cast<std::size_t>(index + code - first));
    }
    index += count;
    first += count;
    first <<= 1U;
    code <<= 1U;
  }
  // NOLINTNEXTLINE(hicpp-exception-baseclass)
  throw std::runtime_error("readPngAsRgb: invalid Huffman code");
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
buildFixedTables(HuffmanTable& litLen, HuffmanTable& dist)
{
  // RFC 1951 3.2.6's fixed code lengths - never transmitted, always these
  // exact values whenever a block uses BTYPE 01.
  std::vector<std::uint8_t> litLenLengths(288);
  std::fill(litLenLengths.begin(), litLenLengths.begin() + 144, 8);
  std::fill(litLenLengths.begin() + 144, litLenLengths.begin() + 256, 9);
  std::fill(litLenLengths.begin() + 256, litLenLengths.begin() + 280, 7);
  std::fill(litLenLengths.begin() + 280, litLenLengths.end(), 8);
  construct(litLen, litLenLengths);

  const std::vector<std::uint8_t> distLengths(30, 5);
  construct(dist, distLengths);
}

void
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
buildDynamicTables(BitReader& reader, HuffmanTable& litLen, HuffmanTable& dist)
{
  // RFC 1951 3.2.7 - the code-length alphabet's own codes arrive in this
  // fixed, deliberately-scrambled order (most-commonly-needed lengths
  // first) rather than symbol order.
  static constexpr std::array<int, 19> codeLengthOrder = {
    16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15
  };

  const auto literalCount = reader.bits(5) + 257;
  const auto distanceCount = reader.bits(5) + 1;
  const auto codeLengthCount = reader.bits(4) + 4;

  std::array<std::uint8_t, 19> codeLengthLengths{};
  for (std::uint32_t i = 0; i < codeLengthCount; ++i) {
    codeLengthLengths.at(static_cast<std::size_t>(codeLengthOrder.at(i))) =
      static_cast<std::uint8_t>(reader.bits(3));
  }
  HuffmanTable codeLengthTable;
  construct(codeLengthTable, codeLengthLengths);

  std::vector<std::uint8_t> lengths(literalCount + distanceCount, 0);
  std::size_t index = 0;
  while (index < lengths.size()) {
    const auto symbol = decodeSymbol(reader, codeLengthTable);
    if (symbol < 16) {
      lengths.at(index++) = static_cast<std::uint8_t>(symbol);
    } else if (symbol == 16) {
      if (index == 0) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error(
          "readPngAsRgb: repeat code with no previous length");
      }
      const auto previous = lengths.at(index - 1);
      auto repeat = reader.bits(2) + 3;
      while (repeat-- != 0) {
        lengths.at(index++) = previous;
      }
    } else if (symbol == 17) {
      auto repeat = reader.bits(3) + 3;
      while (repeat-- != 0) {
        lengths.at(index++) = 0;
      }
    } else if (symbol == 18) {
      auto repeat = reader.bits(7) + 11;
      while (repeat-- != 0) {
        lengths.at(index++) = 0;
      }
    } else {
      // NOLINTNEXTLINE(hicpp-exception-baseclass)
      throw std::runtime_error("readPngAsRgb: invalid code length symbol");
    }
  }

  const std::span<const std::uint8_t> litLenLengths(lengths.data(),
                                                    literalCount);
  // lengths.data()+literalCount stays within lengths: literalCount+
  // distanceCount is exactly lengths.size() (its own construction above),
  // and literalCount alone is always <= that.
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::span<const std::uint8_t> distLengths(lengths.data() + literalCount,
                                                  distanceCount);
  construct(litLen, litLenLengths);
  construct(dist, distLengths);
}

void
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
inflateBlock(BitReader& reader,
             const HuffmanTable& litLen,
             const HuffmanTable& dist,
             std::vector<std::uint8_t>& out)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  // RFC 1951 3.2.5's length/distance extra-bits tables.
  static constexpr std::array<std::uint16_t, 29> lengthBase = {
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258
  };
  static constexpr std::array<std::uint8_t, 29> lengthExtra = {
    0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0
  };
  static constexpr std::array<std::uint16_t, 30> distBase = {
    1,    2,    3,    4,    5,    7,    9,    13,    17,    25,
    33,   49,   65,   97,   129,  193,  257,  385,   513,   769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577
  };
  static constexpr std::array<std::uint8_t, 30> distExtra = {
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13
  };

  for (;;) {
    const auto symbol = decodeSymbol(reader, litLen);
    if (symbol < 256) {
      out.push_back(static_cast<std::uint8_t>(symbol));
    } else if (symbol == 256) {
      return;
    } else {
      const auto lengthIndex = static_cast<std::size_t>(symbol - 257);
      if (lengthIndex >= lengthBase.size()) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: invalid length symbol");
      }
      const auto length =
        lengthBase.at(lengthIndex) + reader.bits(lengthExtra.at(lengthIndex));

      const auto distSymbol = decodeSymbol(reader, dist);
      if (static_cast<std::size_t>(distSymbol) >= distBase.size()) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: invalid distance symbol");
      }
      const auto distance =
        distBase.at(static_cast<std::size_t>(distSymbol)) +
        reader.bits(distExtra.at(static_cast<std::size_t>(distSymbol)));
      if (distance > out.size()) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: back-reference distance "
                                 "past start of output");
      }

      const auto copyFrom = out.size() - distance;
      for (std::uint32_t i = 0; i < length; ++i) {
        // Reads out[copyFrom + i] fresh each iteration (not a cached
        // pointer/iterator, which push_back would invalidate) - when
        // length > distance this deliberately reads bytes this same loop
        // already appended, the standard LZ77 run-length idiom.
        out.push_back(out.at(copyFrom + i));
      }
    }
  }
}

std::vector<std::uint8_t>
inflate(std::span<const std::uint8_t> deflateData)
{
  BitReader reader(deflateData);
  std::vector<std::uint8_t> out;

  bool isFinal = false;
  while (!isFinal) {
    isFinal = reader.bit() != 0;
    const auto blockType = reader.bits(2);
    if (blockType == 0) {
      reader.alignToByte();
      const auto lenLo = reader.byte();
      const auto lenHi = reader.byte();
      const auto nlenLo = reader.byte();
      const auto nlenHi = reader.byte();
      const auto len = static_cast<std::uint16_t>(
        static_cast<unsigned>(lenLo) | (static_cast<unsigned>(lenHi) << 8U));
      const auto notLen = static_cast<std::uint16_t>(
        static_cast<unsigned>(nlenLo) | (static_cast<unsigned>(nlenHi) << 8U));
      if (static_cast<std::uint16_t>(~len) != notLen) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: corrupt stored block length");
      }
      for (std::uint16_t i = 0; i < len; ++i) {
        out.push_back(reader.byte());
      }
    } else if (blockType == 1 || blockType == 2) {
      HuffmanTable litLen;
      HuffmanTable dist;
      if (blockType == 1) {
        buildFixedTables(litLen, dist);
      } else {
        buildDynamicTables(reader, litLen, dist);
      }
      inflateBlock(reader, litLen, dist, out);
    } else {
      // NOLINTNEXTLINE(hicpp-exception-baseclass)
      throw std::runtime_error("readPngAsRgb: invalid DEFLATE block type");
    }
  }
  return out;
}

std::uint8_t
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
paethPredictor(std::uint8_t a, std::uint8_t b, std::uint8_t c)
{
  const auto p =
    static_cast<int>(a) + static_cast<int>(b) - static_cast<int>(c);
  const auto pa = std::abs(p - a);
  const auto pb = std::abs(p - b);
  const auto pc = std::abs(p - c);
  if (pa <= pb && pa <= pc) {
    return a;
  }
  if (pb <= pc) {
    return b;
  }
  return c;
}

// Reverses PNG's per-scanline filtering (each row prefixed by which of the
// 5 filter types it used - see the PNG spec's 9.2/9.3), turning the raw
// inflated bytes into plain packed pixel data. rowBytes/bytesPerPixel are
// taken as already computed by the caller rather than derived from
// width*channels here, since a sub-byte bit depth (grayscale bitDepth < 8)
// packs multiple pixels per byte - rowBytes is then less than width, and
// the PNG spec's own filter byte-distance ("bpp") is still just 1 for those
// depths, not the logical channel count.
std::vector<std::uint8_t>
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
unfilter(std::span<const std::uint8_t> filtered,
         std::size_t rowBytes,
         std::size_t height,
         std::size_t bytesPerPixel)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  std::vector<std::uint8_t> out(rowBytes * height);
  std::vector<std::uint8_t> previousRow(rowBytes, 0);

  std::size_t srcOffset = 0;
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  for (std::size_t y = 0; y < height; ++y) {
    const auto filterType = filtered[srcOffset++];
    const auto src = filtered.subspan(srcOffset, rowBytes);
    srcOffset += rowBytes;
    const std::span<std::uint8_t> dst(out.data() + (y * rowBytes), rowBytes);

    for (std::size_t x = 0; x < rowBytes; ++x) {
      const std::uint8_t a = (x >= bytesPerPixel) ? dst[x - bytesPerPixel] : 0;
      const std::uint8_t b = previousRow[x];
      const std::uint8_t c =
        (x >= bytesPerPixel) ? previousRow[x - bytesPerPixel] : 0;
      switch (filterType) {
        case 0:
          dst[x] = src[x];
          break;
        case 1:
          dst[x] = static_cast<std::uint8_t>(src[x] + a);
          break;
        case 2:
          dst[x] = static_cast<std::uint8_t>(src[x] + b);
          break;
        case 3:
          dst[x] = static_cast<std::uint8_t>(
            src[x] + ((static_cast<unsigned>(a) + b) / 2));
          break;
        case 4:
          dst[x] = static_cast<std::uint8_t>(src[x] + paethPredictor(a, b, c));
          break;
        default:
          // NOLINTNEXTLINE(hicpp-exception-baseclass)
          throw std::runtime_error("readPngAsRgb: invalid PNG filter type");
      }
    }
    previousRow.assign(dst.begin(), dst.end());
  }
  // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access,cppcoreguidelines-pro-bounds-pointer-arithmetic)
  return out;
}

std::size_t
channelsForColorType(std::uint8_t colorType)
{
  switch (colorType) {
    case 0:
      return 1; // grayscale
    case 2:
      return 3; // RGB
    case 4:
      return 2; // grayscale + alpha
    case 6:
      return 4; // RGBA
    default:
      // NOLINTNEXTLINE(hicpp-exception-baseclass)
      throw std::runtime_error(
        "readPngAsRgb: unsupported PNG color type (palette images aren't "
        "supported)");
  }
}

// PNG's own sub-byte-depth packing (spec 2.3): samples are packed MSB-first
// within each byte, and each row starts a fresh byte (any leftover bits at
// a row's end are padding, not the next row's first sample) - both already
// accounted for by unfilter() above treating each row as its own
// rowBytes-sized span. Only grayscale (colorType 0) ever uses a bit depth
// under 8 per the PNG spec (RGB/RGBA/palette are always 8 or 16), so this
// is the only case that needs unpacking; the scale-to-0..255 formula
// (sample * 255 / maxSample) is the PNG spec's own recommended grayscale
// expansion and happens to land exactly on the DMG's 4 LCD shades
// (0x00/0x55/0xAA/0xFF) for bitDepth 2, which is what actually matters here
// - mealybug-tearoom-tests' DMG reference screenshots ship as 2-bit
// grayscale.
std::vector<std::uint8_t>
expandGrayscaleSamples(std::span<const std::uint8_t> packedRows,
                       std::size_t width,
                       std::size_t height,
                       std::uint8_t bitDepth)
{
  const auto rowBytes = (width * bitDepth + 7) / 8;
  const auto maxSample = (1U << bitDepth) - 1U;
  std::vector<std::uint8_t> out(width * height);
  for (std::size_t y = 0; y < height; ++y) {
    const auto row = packedRows.subspan(y * rowBytes, rowBytes);
    for (std::size_t x = 0; x < width; ++x) {
      const auto bitOffset = x * bitDepth;
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      const auto byte = row[bitOffset / 8];
      const auto shift = 8 - bitDepth - (bitOffset % 8);
      const auto sample =
        (static_cast<unsigned>(byte) >> shift) & maxSample;
      out.at((y * width) + x) =
        static_cast<std::uint8_t>((sample * 255U) / maxSample);
    }
  }
  return out;
}

std::vector<std::uint8_t>
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
toRgb(std::span<const std::uint8_t> unfiltered,
      std::size_t width,
      std::size_t height,
      std::uint8_t colorType)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  const auto channels = channelsForColorType(colorType);
  std::vector<std::uint8_t> rgb(width * height * 3);
  for (std::size_t i = 0; i < width * height; ++i) {
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto* pixel = unfiltered.data() + (i * channels);
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    switch (colorType) {
      case 0:
      case 4:
        r = g = b = pixel[0]; // grayscale (alpha, if any, dropped)
        break;
      case 2:
      case 6:
        r = pixel[0];
        g = pixel[1];
        b = pixel[2]; // RGB(A) - alpha, if any, dropped
        break;
      default:
        std::unreachable();
    }
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    rgb.at((i * 3) + 0) = r;
    rgb.at((i * 3) + 1) = g;
    rgb.at((i * 3) + 2) = b;
  }
  return rgb;
}

std::uint32_t
readBigEndianU32(std::span<const std::uint8_t> bytes)
{
  // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
         (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) |
         static_cast<std::uint32_t>(bytes[3]);
  // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
}

}

void
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
writePixelsAsPng(const std::filesystem::path& path,
                 std::span<const std::uint8_t> rgbPixels,
                 std::size_t width,
                 std::size_t height)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  const auto bytesPerRow = width * 3;

  // PNG's uncompressed image data is scanline-oriented: each row gets its
  // own leading filter-type byte (0 = None, i.e. store the row as-is).
  std::vector<std::uint8_t> filtered;
  filtered.reserve((bytesPerRow + 1) * height);
  for (std::size_t row = 0; row < height; ++row) {
    filtered.push_back(0);
    const auto rowStart = rgbPixels.subspan(row * bytesPerRow, bytesPerRow);
    filtered.insert(filtered.end(), rowStart.begin(), rowStart.end());
  }

  // zlib stream wrapping one or more DEFLATE "stored" (uncompressed)
  // blocks, each capped at 65535 bytes: 2-byte zlib header, the stored
  // blocks, then a 4-byte Adler-32 of the uncompressed data.
  std::vector<std::uint8_t> zlibStream{ 0x78, 0x01 };
  constexpr std::size_t maxStoredBlock = 65535;
  for (std::size_t offset = 0; offset < filtered.size();) {
    const auto remaining = filtered.size() - offset;
    const auto blockSize = std::min(remaining, maxStoredBlock);
    const auto isFinalBlock = (offset + blockSize) == filtered.size();
    zlibStream.push_back(isFinalBlock ? 1 : 0);
    const auto len = static_cast<std::uint16_t>(blockSize);
    const auto notLen = static_cast<std::uint16_t>(~len);
    zlibStream.push_back(static_cast<std::uint8_t>(len & 0xFFU));
    zlibStream.push_back(static_cast<std::uint8_t>(len >> 8U));
    zlibStream.push_back(static_cast<std::uint8_t>(notLen & 0xFFU));
    zlibStream.push_back(static_cast<std::uint8_t>(notLen >> 8U));
    const auto block = std::span(filtered).subspan(offset, blockSize);
    zlibStream.insert(zlibStream.end(), block.begin(), block.end());
    offset += blockSize;
  }
  appendBigEndianU32(zlibStream, adler32(filtered));

  std::vector<std::uint8_t> png{ 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };

  std::vector<std::uint8_t> ihdr;
  appendBigEndianU32(ihdr, static_cast<std::uint32_t>(width));
  appendBigEndianU32(ihdr, static_cast<std::uint32_t>(height));
  ihdr.push_back(8); // bit depth
  ihdr.push_back(2); // color type: truecolor (RGB)
  ihdr.push_back(0); // compression method
  ihdr.push_back(0); // filter method
  ihdr.push_back(0); // interlace method
  appendPngChunk(png, "IHDR", ihdr);
  appendPngChunk(png, "IDAT", zlibStream);
  appendPngChunk(png, "IEND", std::span<const std::uint8_t>{});

  std::ofstream file(path, std::ios::binary);
  // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
  file.write(reinterpret_cast<const char*>(png.data()),
             static_cast<std::streamsize>(png.size()));
}

PngImage
readPngAsRgb(const std::filesystem::path& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error("readPngAsRgb: cannot open " + path.string());
  }
  const std::vector<std::uint8_t> bytes{ std::istreambuf_iterator<char>(file),
                                         std::istreambuf_iterator<char>() };

  static constexpr std::array<std::uint8_t, 8> signature = { 0x89, 'P',  'N',
                                                             'G',  '\r', '\n',
                                                             0x1A, '\n' };
  if (bytes.size() < signature.size() ||
      !std::equal(signature.begin(), signature.end(), bytes.begin())) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error("readPngAsRgb: not a PNG file: " + path.string());
  }

  std::optional<std::uint32_t> width;
  std::optional<std::uint32_t> height;
  std::optional<std::uint8_t> colorType;
  std::optional<std::uint8_t> bitDepth;
  std::vector<std::uint8_t> idat;

  std::size_t offset = signature.size();
  while (offset + 12 <= bytes.size()) {
    const auto length = readBigEndianU32(std::span(bytes).subspan(offset, 4));
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
    const std::string_view type(
      reinterpret_cast<const char*>(bytes.data() + offset + 4), 4);
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic,cppcoreguidelines-pro-type-reinterpret-cast)
    const auto dataStart = offset + 8;
    if (dataStart + length + 4 > bytes.size()) {
      // NOLINTNEXTLINE(hicpp-exception-baseclass)
      throw std::runtime_error("readPngAsRgb: truncated chunk in " +
                               path.string());
    }
    const auto data = std::span(bytes).subspan(dataStart, length);

    if (type == "IHDR") {
      if (length != 13) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error("readPngAsRgb: malformed IHDR in " +
                                 path.string());
      }
      width = readBigEndianU32(data.subspan(0, 4));
      height = readBigEndianU32(data.subspan(4, 4));
      // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      bitDepth = data[8];
      colorType = data[9];
      const auto interlaceMethod = data[12];
      // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)
      // Grayscale is the only PNG color type whose bit depth can go under
      // 8 (RGB/RGBA/palette are always 8 or 16 per the spec) - support
      // that down to 1, since mealybug-tearoom-tests' DMG reference
      // screenshots ship as 2-bit grayscale (one of the 4 LCD shades).
      const bool bitDepthSupported =
        *colorType == 0
          ? (*bitDepth == 1 || *bitDepth == 2 || *bitDepth == 4 ||
             *bitDepth == 8)
          : (*bitDepth == 8);
      if (!bitDepthSupported) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error(
          "readPngAsRgb: unsupported bit depth (" +
          std::to_string(*bitDepth) + ") for color type (" +
          std::to_string(*colorType) + ") in " + path.string());
      }
      if (interlaceMethod != 0) {
        // NOLINTNEXTLINE(hicpp-exception-baseclass)
        throw std::runtime_error(
          "readPngAsRgb: interlaced PNGs aren't supported: " + path.string());
      }
    } else if (type == "IDAT") {
      idat.insert(idat.end(), data.begin(), data.end());
    } else if (type == "IEND") {
      break;
    }
    offset = dataStart + length + 4;
  }

  if (!width || !height || !colorType || !bitDepth) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error("readPngAsRgb: missing IHDR in " + path.string());
  }
  if (idat.empty()) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error("readPngAsRgb: missing IDAT in " + path.string());
  }
  // zlib wrapper (RFC 1950): 2-byte header, the DEFLATE stream, then a
  // 4-byte Adler-32 of the uncompressed data - the header/trailer are
  // skipped rather than verified, since a failing inflate() or size check
  // below already catches a corrupt/truncated stream.
  if (idat.size() < 6) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error("readPngAsRgb: IDAT too short in " +
                             path.string());
  }
  // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
  const std::span<const std::uint8_t> deflateData(idat.data() + 2,
                                                  idat.size() - 6);

  const auto channels = channelsForColorType(*colorType);
  const auto rowBytes =
    (static_cast<std::size_t>(*width) * *bitDepth * channels + 7) / 8;
  const auto bytesPerPixel =
    std::max<std::size_t>(1, (static_cast<std::size_t>(*bitDepth) * channels +
                              7) /
                              8);
  const auto expectedFilteredSize = (rowBytes + 1) * *height;

  const auto filtered = inflate(deflateData);
  if (filtered.size() != expectedFilteredSize) {
    // NOLINTNEXTLINE(hicpp-exception-baseclass)
    throw std::runtime_error(
      "readPngAsRgb: decompressed size doesn't match IHDR dimensions in " +
      path.string());
  }

  const auto unfiltered = unfilter(filtered, rowBytes, *height, bytesPerPixel);
  const auto expanded = *bitDepth < 8
                          ? expandGrayscaleSamples(
                              unfiltered, *width, *height, *bitDepth)
                          : unfiltered;
  auto rgb = toRgb(expanded, *width, *height, *colorType);

  return PngImage{ std::move(rgb), *width, *height };
}

bool
// NOLINTBEGIN(bugprone-easily-swappable-parameters)
pixelsMatchPng(std::span<const std::uint8_t> actualRgbPixels,
               std::size_t width,
               std::size_t height,
               const std::filesystem::path& referencePngPath)
// NOLINTEND(bugprone-easily-swappable-parameters)
{
  const auto reference = readPngAsRgb(referencePngPath);
  if (reference.width != width || reference.height != height) {
    return false;
  }
  return std::equal(actualRgbPixels.begin(),
                    actualRgbPixels.end(),
                    reference.rgbPixels.begin(),
                    reference.rgbPixels.end());
}
