export module png;

import std;

export void
writePixelsAsPng(const std::filesystem::path& path,
                 std::span<const std::uint8_t> rgbPixels,
                 std::size_t width,
                 std::size_t height);

export struct PngImage
{
  std::vector<std::uint8_t> rgbPixels;
  std::size_t width;
  std::size_t height;
};

export PngImage
readPngAsRgb(const std::filesystem::path& path);

export bool
pixelsMatchPng(std::span<const std::uint8_t> actualRgbPixels,
               std::size_t width,
               std::size_t height,
               const std::filesystem::path& referencePngPath);
