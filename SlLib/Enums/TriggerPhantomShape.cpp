#include "TriggerPhantomShape.hpp"

namespace SlLib::Enums {

std::string toString(TriggerPhantomShape shape)
{
    switch (shape) {
    case TriggerPhantomShape::Sphere:
        return "Sphere";
    case TriggerPhantomShape::Box:
        return "Box";
    case TriggerPhantomShape::CylinderX:
        return "CylinderX";
    case TriggerPhantomShape::CylinderY:
        return "CylinderY";
    case TriggerPhantomShape::CylinderZ:
        return "CylinderZ";
    }

    return "Unknown";
}

template <>
std::optional<TriggerPhantomShape> fromString<TriggerPhantomShape>(std::string_view text)
{
    return fromStringFromAll<TriggerPhantomShape>(allTriggerPhantomShapes, text);
}

} // namespace SlLib::Enums
