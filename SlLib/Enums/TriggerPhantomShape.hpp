#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class TriggerPhantomShape
{
    Sphere,
    Box,
    CylinderX,
    CylinderY,
    CylinderZ,
};

inline constexpr auto allTriggerPhantomShapes() noexcept -> std::array<TriggerPhantomShape, 5>
{
    return {TriggerPhantomShape::Sphere,
            TriggerPhantomShape::Box,
            TriggerPhantomShape::CylinderX,
            TriggerPhantomShape::CylinderY,
            TriggerPhantomShape::CylinderZ};
}

std::string toString(TriggerPhantomShape shape);

template <>
std::optional<TriggerPhantomShape> fromString<TriggerPhantomShape>(std::string_view text);

inline std::optional<TriggerPhantomShape> fromStringTriggerPhantomShape(std::string_view text)
{
    return fromString<TriggerPhantomShape>(text);
}

} // namespace SlLib::Enums
