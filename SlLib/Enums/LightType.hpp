#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class LightType
{
    Ambient,
    Directional,
    Point,
    Spot,
};

inline constexpr auto allLightTypes() noexcept -> std::array<LightType, 4>
{
    return {LightType::Ambient, LightType::Directional, LightType::Point, LightType::Spot};
}

std::string toString(LightType type);

template <>
std::optional<LightType> fromString<LightType>(std::string_view text);

inline std::optional<LightType> fromStringLightType(std::string_view text)
{
    return fromString<LightType>(text);
}

} // namespace SlLib::Enums
