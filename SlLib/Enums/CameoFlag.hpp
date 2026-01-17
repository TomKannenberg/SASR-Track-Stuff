#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class CameoFlag : std::uint32_t
{
    None = 0,
    RespawnAtEndOfSpline = 1 << 0,
    AddRemainingTimeToSpawn = 1 << 1,
    NeverRespawn = 1 << 2,
    SplineTrackLeaderSpeed = 1 << 3,
    DontMoveOnSplineIdle = 1 << 4,
    DisableSplineMovement = 1 << 5,
    UseSplineRollValue = 1 << 6,
    AttackActive = 1 << 7,
};

constexpr CameoFlag operator|(CameoFlag lhs, CameoFlag rhs) noexcept
{
    return static_cast<CameoFlag>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr CameoFlag operator&(CameoFlag lhs, CameoFlag rhs) noexcept
{
    return static_cast<CameoFlag>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr CameoFlag operator~(CameoFlag flags) noexcept
{
    return static_cast<CameoFlag>(~static_cast<std::uint32_t>(flags));
}

constexpr CameoFlag& operator|=(CameoFlag& lhs, CameoFlag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr CameoFlag& operator&=(CameoFlag& lhs, CameoFlag rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr auto allCameoFlags() noexcept -> std::array<CameoFlag, 8>
{
    return {
        CameoFlag::RespawnAtEndOfSpline,
        CameoFlag::AddRemainingTimeToSpawn,
        CameoFlag::NeverRespawn,
        CameoFlag::SplineTrackLeaderSpeed,
        CameoFlag::DontMoveOnSplineIdle,
        CameoFlag::DisableSplineMovement,
        CameoFlag::UseSplineRollValue,
        CameoFlag::AttackActive,
    };
}

std::string toString(CameoFlag flags);

template <>
std::optional<CameoFlag> fromString<CameoFlag>(std::string_view text);

inline std::optional<CameoFlag> fromStringCameoFlag(std::string_view text)
{
    return fromString<CameoFlag>(text);
}

} // namespace SlLib::Enums
