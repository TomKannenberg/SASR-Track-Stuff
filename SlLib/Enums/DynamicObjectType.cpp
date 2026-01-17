#include "DynamicObjectType.hpp"

namespace SlLib::Enums {

std::string toString(DynamicObjectType type)
{
    switch (type) {
    case DynamicObjectType::WaterFloat:
        return "WaterFloat";
    case DynamicObjectType::ChevronController:
        return "ChevronController";
    case DynamicObjectType::FormationController:
        return "FormationController";
    case DynamicObjectType::SonicRing:
        return "SonicRing";
    }

    return "Unknown";
}

template <>
std::optional<DynamicObjectType> fromString<DynamicObjectType>(std::string_view text)
{
    return fromStringFromAll<DynamicObjectType>(allDynamicObjectTypes, text);
}

} // namespace SlLib::Enums
