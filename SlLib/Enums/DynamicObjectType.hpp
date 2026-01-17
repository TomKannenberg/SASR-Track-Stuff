#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class DynamicObjectType
{
    WaterFloat,
    ChevronController,
    FormationController,
    SonicRing,
};

inline constexpr auto allDynamicObjectTypes() noexcept -> std::array<DynamicObjectType, 4>
{
    return {DynamicObjectType::WaterFloat,
            DynamicObjectType::ChevronController,
            DynamicObjectType::FormationController,
            DynamicObjectType::SonicRing};
}

std::string toString(DynamicObjectType type);

template <>
std::optional<DynamicObjectType> fromString<DynamicObjectType>(std::string_view text);

inline std::optional<DynamicObjectType> fromStringDynamicObjectType(std::string_view text)
{
    return fromString<DynamicObjectType>(text);
}

} // namespace SlLib::Enums
