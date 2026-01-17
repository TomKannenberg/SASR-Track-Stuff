#include "CameoType.hpp"

namespace SlLib::Enums {

std::string toString(CameoType type)
{
    switch (type) {
    case CameoType::StaticFidgetFacePlayer:
        return "StaticFidgetFacePlayer";
    case CameoType::AnimatedHazard:
        return "AnimatedHazard";
    case CameoType::AnimatedDropper:
        return "AnimatedDropper";
    case CameoType::AnimatedDropperFollowTrack:
        return "AnimatedDropperFollowTrack";
    case CameoType::StaticFidget:
        return "StaticFidget";
    case CameoType::AttackerFollowTrack:
        return "AttackerFollowTrack";
    case CameoType::AttackStatic:
        return "AttackStatic";
    case CameoType::Flagman:
        return "Flagman";
    }

    return "Unknown";
}

template <>
std::optional<CameoType> fromString<CameoType>(std::string_view text)
{
    return fromStringFromAll<CameoType>(allCameoTypes, text);
}

} // namespace SlLib::Enums
