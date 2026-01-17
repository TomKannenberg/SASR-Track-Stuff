#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class LightDataFlag : std::uint32_t
{
    None = 0,
    BoxLight = 1 << 0,
    ShadowLight = 1 << 7,
    LightGroup0 = 1 << 15,
    LightGroup1 = 1 << 16,
    LightGroup2 = 1 << 17,
    LightGroup3 = 1 << 18,
    LightGroup4 = 1 << 19,
    LightGroup5 = 1 << 20,
    LightUser0 = 1 << 23,
    LightUser1 = 1 << 24,
    LightOverlays = 1 << 25,
};

constexpr LightDataFlag operator|(LightDataFlag lhs, LightDataFlag rhs) noexcept
{
    return static_cast<LightDataFlag>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr LightDataFlag operator&(LightDataFlag lhs, LightDataFlag rhs) noexcept
{
    return static_cast<LightDataFlag>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr LightDataFlag operator~(LightDataFlag flags) noexcept
{
    return static_cast<LightDataFlag>(~static_cast<std::uint32_t>(flags));
}

constexpr LightDataFlag& operator|=(LightDataFlag& lhs, LightDataFlag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr LightDataFlag& operator&=(LightDataFlag& lhs, LightDataFlag rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr auto allLightDataFlags() noexcept -> std::array<LightDataFlag, 11>
{
    return {LightDataFlag::BoxLight,
            LightDataFlag::ShadowLight,
            LightDataFlag::LightGroup0,
            LightDataFlag::LightGroup1,
            LightDataFlag::LightGroup2,
            LightDataFlag::LightGroup3,
            LightDataFlag::LightGroup4,
            LightDataFlag::LightGroup5,
            LightDataFlag::LightUser0,
            LightDataFlag::LightUser1,
            LightDataFlag::LightOverlays};
}

std::string toString(LightDataFlag flags);

template <>
std::optional<LightDataFlag> fromString<LightDataFlag>(std::string_view text);

inline std::optional<LightDataFlag> fromStringLightDataFlag(std::string_view text)
{
    return fromString<LightDataFlag>(text);
}

} // namespace SlLib::Enums
