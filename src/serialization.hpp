#pragma once

// GCC's experimental std module is built without -freflection, which omits
// these <meta> declarations. Declare the small standard reflection surface
// used below without textually including <meta> alongside import std.
namespace std::meta {
using info = decltype(^^int);

struct access_context
{
  static consteval access_context current() noexcept;

  info scope;
  info designatingClass;
};

consteval std::vector<info> nonstatic_data_members_of(info, access_context);
}

namespace gbemu {

template<typename T>
void serialize(SaveStateWriter& writer, const T& value)
{
  using Value = std::remove_cv_t<T>;

  if constexpr (requires { value.serialize(writer); }) {
    value.serialize(writer);
  } else if constexpr (std::same_as<Value, bool>) {
    writer.writeBool(value);
  } else if constexpr (std::same_as<Value, float>) {
    writer.writeFloat(value);
  } else if constexpr (std::same_as<Value, std::size_t>) {
    writer.writeSize(value);
  } else if constexpr (std::integral<Value>) {
    using Unsigned = std::make_unsigned_t<Value>;
    const auto unsignedValue = static_cast<Unsigned>(value);

    if constexpr (sizeof(Value) == sizeof(std::uint8_t)) {
      writer.writeU8(static_cast<std::uint8_t>(unsignedValue));
    } else if constexpr (sizeof(Value) == sizeof(std::uint16_t)) {
      writer.writeU16(static_cast<std::uint16_t>(unsignedValue));
    } else if constexpr (sizeof(Value) == sizeof(std::uint32_t)) {
      writer.writeU32(static_cast<std::uint32_t>(unsignedValue));
    } else if constexpr (sizeof(Value) == sizeof(std::uint64_t)) {
      writer.writeU64(static_cast<std::uint64_t>(unsignedValue));
    } else {
      static_assert(false, "serialize does not support this integer width");
    }
  } else if constexpr (std::is_enum_v<Value>) {
    serialize(writer, static_cast<std::underlying_type_t<Value>>(value));
  } else if constexpr (std::is_class_v<Value>) {
    template for (constexpr auto member :
                  std::meta::nonstatic_data_members_of(
                    ^^Value, std::meta::access_context::current())) {
      serialize(writer, value.[:member:]);
    }
  } else {
    static_assert(false, "serialize does not support this type");
  }
}

template<typename T>
void deserialize(SaveStateReader& reader, T& value)
{
  using Value = std::remove_cv_t<T>;

  if constexpr (requires { value.deserialize(reader); }) {
    value.deserialize(reader);
  } else if constexpr (std::same_as<Value, bool>) {
    value = reader.readBool();
  } else if constexpr (std::same_as<Value, float>) {
    value = reader.readFloat();
  } else if constexpr (std::same_as<Value, std::size_t>) {
    value = reader.readSize();
  } else if constexpr (std::integral<Value>) {
    using Unsigned = std::make_unsigned_t<Value>;
    Unsigned unsignedValue{};

    if constexpr (sizeof(Value) == sizeof(std::uint8_t)) {
      unsignedValue = reader.readU8();
    } else if constexpr (sizeof(Value) == sizeof(std::uint16_t)) {
      unsignedValue = reader.readU16();
    } else if constexpr (sizeof(Value) == sizeof(std::uint32_t)) {
      unsignedValue = reader.readU32();
    } else if constexpr (sizeof(Value) == sizeof(std::uint64_t)) {
      unsignedValue = reader.readU64();
    } else {
      static_assert(false, "deserialize does not support this integer width");
    }

    value = std::bit_cast<Value>(unsignedValue);
  } else if constexpr (std::is_enum_v<Value>) {
    std::underlying_type_t<Value> underlying{};
    deserialize(reader, underlying);
    value = static_cast<Value>(underlying);
  } else if constexpr (std::is_class_v<Value>) {
    template for (constexpr auto member :
                  std::meta::nonstatic_data_members_of(
                    ^^Value, std::meta::access_context::current())) {
      deserialize(reader, value.[:member:]);
    }
  } else {
    static_assert(false, "deserialize does not support this type");
  }
}

}
