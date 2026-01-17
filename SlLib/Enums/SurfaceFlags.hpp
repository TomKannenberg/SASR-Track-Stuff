#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class SurfaceFlags : std::uint32_t
{
    None = 0,
    Sticky = 0x1,
    WeakSticky = 0x20,
    DoubleGravity = 0x40,
    SpaceWarp = 0x80,
    CatchupDisabled = 0x10000000,
    StuntsDisabled = 0x20000000,
    BoostDisabled = 0x40000000,
};

constexpr SurfaceFlags operator|(SurfaceFlags lhs, SurfaceFlags rhs) noexcept
{
    return static_cast<SurfaceFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr SurfaceFlags operator&(SurfaceFlags lhs, SurfaceFlags rhs) noexcept
{
    return static_cast<SurfaceFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr SurfaceFlags operator~(SurfaceFlags flags) noexcept
{
    return static_cast<SurfaceFlags>(~static_cast<std::uint32_t>(flags));
}

constexpr SurfaceFlags& operator|=(SurfaceFlags& lhs, SurfaceFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr SurfaceFlags& operator&=(SurfaceFlags& lhs, SurfaceFlags rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

std::string toString(SurfaceFlags flags);

template <>
std::optional<SurfaceFlags> fromString<SurfaceFlags>(std::string_view text);

inline std::optional<SurfaceFlags> fromStringSurfaceFlags(std::string_view text)
{
    return fromString<SurfaceFlags>(text);
}

} // namespace SlLib::Enums
