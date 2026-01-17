#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class SurfaceType
{
    None,
    Grass,
    DeepGrass,
    Mud,
    Wood,
    Cobbles,
    Concrete,
    Seabed,
    Tarmac,
    Ice,
    Dirt,
    DirtGravel,
    MetalGrating,
    Metal,
    Rock,
    Snow,
    SpaceRoad,
    Slate = 18,
    Stairs,
    Glass,
};

inline constexpr auto allSurfaceTypes() noexcept -> std::array<SurfaceType, 20>
{
    return {
        SurfaceType::None,
        SurfaceType::Grass,
        SurfaceType::DeepGrass,
        SurfaceType::Mud,
        SurfaceType::Wood,
        SurfaceType::Cobbles,
        SurfaceType::Concrete,
        SurfaceType::Seabed,
        SurfaceType::Tarmac,
        SurfaceType::Ice,
        SurfaceType::Dirt,
        SurfaceType::DirtGravel,
        SurfaceType::MetalGrating,
        SurfaceType::Metal,
        SurfaceType::Rock,
        SurfaceType::Snow,
        SurfaceType::SpaceRoad,
        SurfaceType::Slate,
        SurfaceType::Stairs,
        SurfaceType::Glass,
    };
}

std::string toString(SurfaceType type);

template <>
std::optional<SurfaceType> fromString<SurfaceType>(std::string_view text);

inline std::optional<SurfaceType> fromStringSurfaceType(std::string_view text)
{
    return fromString<SurfaceType>(text);
}

} // namespace SlLib::Enums
