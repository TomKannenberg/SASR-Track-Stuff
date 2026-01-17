#include "SurfaceType.hpp"

#include <array>

namespace SlLib::Enums {

namespace {

inline constexpr auto surfaceTypeMappings()
{
    return std::array{
        std::pair{SurfaceType::None, "None"},
        std::pair{SurfaceType::Grass, "Grass"},
        std::pair{SurfaceType::DeepGrass, "DeepGrass"},
        std::pair{SurfaceType::Mud, "Mud"},
        std::pair{SurfaceType::Wood, "Wood"},
        std::pair{SurfaceType::Cobbles, "Cobbles"},
        std::pair{SurfaceType::Concrete, "Concrete"},
        std::pair{SurfaceType::Seabed, "Seabed"},
        std::pair{SurfaceType::Tarmac, "Tarmac"},
        std::pair{SurfaceType::Ice, "Ice"},
        std::pair{SurfaceType::Dirt, "Dirt"},
        std::pair{SurfaceType::DirtGravel, "DirtGravel"},
        std::pair{SurfaceType::MetalGrating, "MetalGrating"},
        std::pair{SurfaceType::Metal, "Metal"},
        std::pair{SurfaceType::Rock, "Rock"},
        std::pair{SurfaceType::Snow, "Snow"},
        std::pair{SurfaceType::SpaceRoad, "SpaceRoad"},
        std::pair{SurfaceType::Slate, "Slate"},
        std::pair{SurfaceType::Stairs, "Stairs"},
        std::pair{SurfaceType::Glass, "Glass"},
    };
}

} // namespace

std::string toString(SurfaceType type)
{
    for (auto [value, label] : surfaceTypeMappings()) {
        if (value == type) {
            return std::string{label};
        }
    }

    return "Unknown";
}

template <>
std::optional<SurfaceType> fromString<SurfaceType>(std::string_view text)
{
    for (auto [value, label] : surfaceTypeMappings()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
