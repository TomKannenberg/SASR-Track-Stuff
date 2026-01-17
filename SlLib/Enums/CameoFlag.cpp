#include "CameoFlag.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto cameoFlagLabels()
{
    return std::array{
        std::pair{CameoFlag::RespawnAtEndOfSpline, "RespawnAtEndOfSpline"},
        std::pair{CameoFlag::AddRemainingTimeToSpawn, "AddRemainingTimeToSpawn"},
        std::pair{CameoFlag::NeverRespawn, "NeverRespawn"},
        std::pair{CameoFlag::SplineTrackLeaderSpeed, "SplineTrackLeaderSpeed"},
        std::pair{CameoFlag::DontMoveOnSplineIdle, "DontMoveOnSplineIdle"},
        std::pair{CameoFlag::DisableSplineMovement, "DisableSplineMovement"},
        std::pair{CameoFlag::UseSplineRollValue, "UseSplineRollValue"},
        std::pair{CameoFlag::AttackActive, "AttackActive"},
    };
}

} // namespace

std::string toString(CameoFlag flags)
{
    if (flags == CameoFlag::None) {
        return "None";
    }

    std::string result;
    for (auto [value, label] : cameoFlagLabels()) {
        if ((flags & value) != CameoFlag::None) {
            if (!result.empty()) {
                result += '|';
            }
            result += label;
        }
    }

    return result.empty() ? "None" : result;
}

template <>
std::optional<CameoFlag> fromString<CameoFlag>(std::string_view text)
{
    for (auto [value, label] : cameoFlagLabels()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
