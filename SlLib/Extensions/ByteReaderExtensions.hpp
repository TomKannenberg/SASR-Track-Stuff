#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <codecvt>
#include <locale>
#include <span>
#include <stdexcept>
#include <string>

namespace SlLib::Extensions {

inline void ensureAvailable(std::size_t length, std::size_t offset, std::size_t required)
{
    if (offset + required > length) {
        throw std::out_of_range("Buffer is too small for requested read.");
    }
}

inline std::int32_t readInt32(std::span<const std::uint8_t> data, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 4);
    return static_cast<std::int32_t>(data[offset]) |
           (static_cast<std::int32_t>(data[offset + 1]) << 8) |
           (static_cast<std::int32_t>(data[offset + 2]) << 16) |
           (static_cast<std::int32_t>(data[offset + 3]) << 24);
}

inline std::int32_t readInt32BigEndian(std::span<const std::uint8_t> data, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 4);
    return static_cast<std::int32_t>(data[offset + 3]) |
           (static_cast<std::int32_t>(data[offset + 2]) << 8) |
           (static_cast<std::int32_t>(data[offset + 1]) << 16) |
           (static_cast<std::int32_t>(data[offset + 0]) << 24);
}

inline float readFloat(std::span<const std::uint8_t> data, std::size_t offset)
{
    return std::bit_cast<float>(readInt32(data, offset));
}

inline std::string readString(std::span<const std::uint8_t> data, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 0);
    std::size_t terminator = offset;
    while (terminator < data.size() && data[terminator] != 0) {
        ++terminator;
    }

    if (terminator == offset) {
        return {};
    }

    return std::string(reinterpret_cast<const char*>(data.data() + offset), terminator - offset);
}

inline std::string readTerminatedUnicodeString(std::span<const std::uint8_t> data, std::size_t offset)
{
    ensureAvailable(data.size(), offset, 0);
    std::u16string buffer;
    std::size_t cursor = offset;

    while (cursor + 1 < data.size()) {
        char16_t value = static_cast<char16_t>(data[cursor]) |
                        (static_cast<char16_t>(data[cursor + 1]) << 8);

        cursor += 2;
        if (value == 0) {
            break;
        }

        buffer.push_back(value);
    }

    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(buffer);
}

} // namespace SlLib::Extensions
