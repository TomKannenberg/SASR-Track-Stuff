#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>

#include "SlLib/Math/Vector.hpp"

namespace SlLib::Extensions {

inline void ensureAvailable(std::size_t length, std::size_t offset, std::size_t required)
{
    if (offset + required > length) {
        throw std::out_of_range("Buffer is too small for requested write.");
    }
}

inline void writeBoolean(std::span<std::uint8_t> data, bool value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 1);
    data[offset] = static_cast<std::uint8_t>(value ? 1 : 0);
}

inline void writeUint8(std::span<std::uint8_t> data, std::uint8_t value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 1);
    data[offset] = value;
}

inline void writeInt16(std::span<std::uint8_t> data, std::int16_t value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, sizeof(std::int16_t));
    auto ptr = reinterpret_cast<std::uint8_t*>(&value);
    data[offset + 0] = ptr[0];
    data[offset + 1] = ptr[1];
}

inline void writeUint16(std::span<std::uint8_t> data, std::uint16_t value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, sizeof(std::uint16_t));
    auto ptr = reinterpret_cast<const std::uint8_t*>(&value);
    data[offset + 0] = ptr[0];
    data[offset + 1] = ptr[1];
}

inline void writeInt32(std::span<std::uint8_t> data, std::int32_t value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, sizeof(std::int32_t));
    auto ptr = reinterpret_cast<const std::uint8_t*>(&value);
    data[offset + 0] = ptr[0];
    data[offset + 1] = ptr[1];
    data[offset + 2] = ptr[2];
    data[offset + 3] = ptr[3];
}

inline void writeFloat(std::span<std::uint8_t> data, float value, std::size_t offset)
{
    ensureAvailable(data.size(), offset, sizeof(float));
    std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
    writeInt32(data, static_cast<std::int32_t>(bits), offset);
}

inline void writeFloat2(std::span<std::uint8_t> data, Math::Vector2 const& value, std::size_t offset)
{
    writeFloat(data, value.X, offset);
    writeFloat(data, value.Y, offset + 4);
}

inline void writeFloat3(std::span<std::uint8_t> data, Math::Vector3 const& value, std::size_t offset)
{
    writeFloat(data, value.X, offset);
    writeFloat(data, value.Y, offset + 4);
    writeFloat(data, value.Z, offset + 8);
}

inline void writeFloat4(std::span<std::uint8_t> data, Math::Vector4 const& value, std::size_t offset)
{
    writeFloat(data, value.X, offset);
    writeFloat(data, value.Y, offset + 4);
    writeFloat(data, value.Z, offset + 8);
    writeFloat(data, value.W, offset + 12);
}

inline void writeMatrix(std::span<std::uint8_t> data, Math::Matrix4x4 const& value, std::size_t offset)
{
    for (std::size_t i = 0; i < 4; ++i) {
        for (std::size_t j = 0; j < 4; ++j) {
            writeFloat(data, value(i, j), offset + i * 16 + j * 4);
        }
    }
}

inline void writeString(std::span<std::uint8_t> data, std::string_view value, std::size_t offset)
{
    std::size_t length = value.size();
    ensureAvailable(data.size(), offset, length + 1);
    std::memcpy(data.data() + offset, value.data(), length);
    data[offset + length] = 0;
}

} // namespace SlLib::Extensions
