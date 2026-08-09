export module png;

import std;

// Debugging aid for visually inspecting a failing frame comparison in
// gameboy_test.cpp - writes an 8-bit RGB pixel buffer (row-major, no
// padding - exactly EmulationFrame::pixels' and the dmg-acid2/cgb-acid2/
// mbc3-tester reference.rgb files' own layout) out as a real, viewable PNG
// file. No libpng/zlib dependency: DEFLATE's "stored" block type needs no
// actual compression, so this hand-assembles just enough of the
// PNG/zlib/DEFLATE container format around the raw bytes to be valid. Not
// part of any assertion - call it by hand (e.g. from a temporary line in a
// failing TEST_CASE) pointed at frame.pixels and/or the reference buffer
// it's compared against.
export void
writePixelsAsPng(const std::filesystem::path& path,
                 std::span<const std::uint8_t> rgbPixels,
                 std::size_t width,
                 std::size_t height);

export struct PngImage
{
  // width*height*3 bytes, 8-bit RGB, row-major, no padding - same layout
  // writePixelsAsPng() above and EmulationFrame::pixels both use. Any
  // alpha channel in the source PNG is dropped (Game Boy screenshots are
  // always fully opaque); grayscale sources are expanded to RGB.
  std::vector<std::uint8_t> rgbPixels;
  std::size_t width;
  std::size_t height;
};

// Decodes an 8-bit, non-interlaced PNG (grayscale, grayscale+alpha, RGB, or
// RGBA - palette/indexed-color PNGs and bit depths other than 8 aren't
// supported and throw) into raw RGB pixels. A real DEFLATE/zlib inflate
// implementation, not just writePixelsAsPng()'s "stored block" special
// case - real-world PNGs like the game-boy-test-roms release's own
// reference screenshots are actually compressed. Throws
// std::runtime_error on any malformed or unsupported input.
export PngImage
readPngAsRgb(const std::filesystem::path& path);

// Decodes referencePngPath and compares it against actualRgbPixels
// (width*height*3, 8-bit RGB, row-major, no padding, e.g. an
// EmulationFrame::pixels span) - false on any dimension or pixel mismatch,
// true only on an exact match.
export bool
pixelsMatchPng(std::span<const std::uint8_t> actualRgbPixels,
               std::size_t width,
               std::size_t height,
               const std::filesystem::path& referencePngPath);
