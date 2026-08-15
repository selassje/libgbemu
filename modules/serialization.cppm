export module gbemu:serialization;

import std;

export namespace gbemu {

class SaveStateWriter
{
public:
  void writeU8(std::uint8_t value);
  void writeU16(std::uint16_t value);
  void writeU32(std::uint32_t value);
  void writeU64(std::uint64_t value);
  void writeSize(std::size_t value);
  void writeBool(bool value);
  void writeFloat(float value);
  void writeBytes(std::span<const std::uint8_t> bytes);

  [[nodiscard]] const std::vector<std::uint8_t>& bytes() const;

private:
  std::vector<std::uint8_t> m_bytes;
};

class SaveStateReader
{
public:
  explicit SaveStateReader(std::span<const std::uint8_t> data);

  std::uint8_t readU8();
  std::uint16_t readU16();
  std::uint32_t readU32();
  std::uint64_t readU64();
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
