#include "ParticleForceMode.hpp"

namespace SlLib::Enums {

std::string toString(ParticleForceMode mode)
{
    switch (mode) {
    case ParticleForceMode::Gravity:
        return "Gravity";
    case ParticleForceMode::Direction:
        return "Direction";
    case ParticleForceMode::Point:
        return "Point";
    case ParticleForceMode::Twist:
        return "Twist";
    }

    return "Unknown";
}

template <>
std::optional<ParticleForceMode> fromString<ParticleForceMode>(std::string_view text)
{
    return fromStringFromAll<ParticleForceMode>(allParticleForceModes, text);
}

} // namespace SlLib::Enums
