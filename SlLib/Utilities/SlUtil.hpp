#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace SlLib::Utilities {

inline void jenkinsMix(uint32_t& a, uint32_t& b, uint32_t& c)
{
    a -= b; a -= c; a ^= c >> 13;
    b -= c; b -= a; b ^= a << 8;
    c -= a; c -= b; c ^= b >> 13;
    a -= b; a -= c; a ^= c >> 12;
    b -= c; b -= a; b ^= a << 16;
    c -= a; c -= b; c ^= b >> 5;
    a -= b; a -= c; a ^= c >> 3;
    b -= c; b -= a; b ^= a << 10;
    c -= a; c -= b; c ^= b >> 15;
}

inline std::int32_t sumoHash(std::string_view input)
{
    const auto* data = reinterpret_cast<const std::uint8_t*>(input.data());
    uint32_t len = static_cast<uint32_t>(input.size());
    uint32_t offset = 0;
    uint32_t a = 0x9e3779b9u;
    uint32_t b = a;
    uint32_t c = 0x4c11db7u;

    while (len >= 12) {
        a += static_cast<uint32_t>(data[offset + 0]) |
             (static_cast<uint32_t>(data[offset + 1]) << 8) |
             (static_cast<uint32_t>(data[offset + 2]) << 16) |
             (static_cast<uint32_t>(data[offset + 3]) << 24);
        b += static_cast<uint32_t>(data[offset + 4]) |
             (static_cast<uint32_t>(data[offset + 5]) << 8) |
             (static_cast<uint32_t>(data[offset + 6]) << 16) |
             (static_cast<uint32_t>(data[offset + 7]) << 24);
        c += static_cast<uint32_t>(data[offset + 8]) |
             (static_cast<uint32_t>(data[offset + 9]) << 8) |
             (static_cast<uint32_t>(data[offset + 10]) << 16) |
             (static_cast<uint32_t>(data[offset + 11]) << 24);

        jenkinsMix(a, b, c);
        offset += 12;
        len -= 12;
    }

    c += static_cast<uint32_t>(input.size());

    switch (len) {
    case 11:
        c += static_cast<uint32_t>(data[offset + 10]) << 24;
        [[fallthrough]];
    case 10:
        c += static_cast<uint32_t>(data[offset + 9]) << 16;
        [[fallthrough]];
    case 9:
        c += static_cast<uint32_t>(data[offset + 8]) << 8;
        [[fallthrough]];
    case 8:
        b += static_cast<uint32_t>(data[offset + 7]) << 24;
        [[fallthrough]];
    case 7:
        b += static_cast<uint32_t>(data[offset + 6]) << 16;
        [[fallthrough]];
    case 6:
        b += static_cast<uint32_t>(data[offset + 5]) << 8;
        [[fallthrough]];
    case 5:
        b += static_cast<uint32_t>(data[offset + 4]);
        [[fallthrough]];
    case 4:
        a += static_cast<uint32_t>(data[offset + 3]) << 24;
        [[fallthrough]];
    case 3:
        a += static_cast<uint32_t>(data[offset + 2]) << 16;
        [[fallthrough]];
    case 2:
        a += static_cast<uint32_t>(data[offset + 1]) << 8;
        [[fallthrough]];
    case 1:
        a += static_cast<uint32_t>(data[offset + 0]);
        break;
    default:
        break;
    }

    jenkinsMix(a, b, c);
    return static_cast<std::int32_t>(c);
}

inline std::size_t align(std::size_t value, std::size_t boundary)
{
    if (boundary == 0) {
        return value;
    }

    return ((value + boundary - 1) / boundary) * boundary;
}

int HashString(std::string_view input) noexcept;

} // namespace SlLib::Utilities
