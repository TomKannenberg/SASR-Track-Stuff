#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class ParticleForceMode
{
    Gravity,
    Direction,
    Point,
    Twist,
};

inline constexpr auto allParticleForceModes() noexcept -> std::array<ParticleForceMode, 4>
{
    return {ParticleForceMode::Gravity, ParticleForceMode::Direction, ParticleForceMode::Point, ParticleForceMode::Twist};
}

std::string toString(ParticleForceMode mode);

template <>
std::optional<ParticleForceMode> fromString<ParticleForceMode>(std::string_view text);

inline std::optional<ParticleForceMode> fromStringParticleForceMode(std::string_view text)
{
    return fromString<ParticleForceMode>(text);
}

} // namespace SlLib::Enums
