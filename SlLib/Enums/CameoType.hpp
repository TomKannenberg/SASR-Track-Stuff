#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class CameoType
{
    StaticFidgetFacePlayer,
    AnimatedHazard,
    AnimatedDropper,
    AnimatedDropperFollowTrack,
    StaticFidget,
    AttackerFollowTrack,
    AttackStatic,
    Flagman,
};

inline constexpr auto allCameoTypes() noexcept -> std::array<CameoType, 8>
{
    return {CameoType::StaticFidgetFacePlayer,
            CameoType::AnimatedHazard,
            CameoType::AnimatedDropper,
            CameoType::AnimatedDropperFollowTrack,
            CameoType::StaticFidget,
            CameoType::AttackerFollowTrack,
            CameoType::AttackStatic,
            CameoType::Flagman};
}

std::string toString(CameoType type);

template <>
std::optional<CameoType> fromString<CameoType>(std::string_view text);

inline std::optional<CameoType> fromStringCameoType(std::string_view text)
{
    return fromString<CameoType>(text);
}

} // namespace SlLib::Enums
