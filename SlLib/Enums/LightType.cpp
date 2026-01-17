#include "LightType.hpp"

namespace SlLib::Enums {

std::string toString(LightType type)
{
    switch (type) {
    case LightType::Ambient:
        return "Ambient";
    case LightType::Directional:
        return "Directional";
    case LightType::Point:
        return "Point";
    case LightType::Spot:
        return "Spot";
    }

    return "Unknown";
}

template <>
std::optional<LightType> fromString<LightType>(std::string_view text)
{
    return fromStringFromAll<LightType>(allLightTypes, text);
}

} // namespace SlLib::Enums
