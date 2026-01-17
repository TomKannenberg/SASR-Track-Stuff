#include "VehicleFlags.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto vehicleFlagMappings()
{
    return std::array{
        std::pair{VehicleFlags::TriggerCar, "TriggerCar"},
        std::pair{VehicleFlags::TriggerBoat, "TriggerBoat"},
        std::pair{VehicleFlags::TriggerPlane, "TriggerPlane"},
        std::pair{VehicleFlags::TriggerWeapon, "TriggerWeapon"},
        std::pair{VehicleFlags::TriggerOnLoad, "TriggerOnLoad"},
        std::pair{VehicleFlags::TriggerPrediction, "TriggerPrediction"},
        std::pair{VehicleFlags::TriggerHasJumboMap, "TriggerHasJumboMap"},
        std::pair{VehicleFlags::TriggerOncePerRacer, "TriggerOncePerRacer"},
    };
}

} // namespace

std::string toString(VehicleFlags flags)
{
    if (flags == VehicleFlags::None) {
        return "None";
    }

    std::string result;
    for (auto [value, label] : vehicleFlagMappings()) {
        if ((flags & value) != VehicleFlags::None) {
            if (!result.empty()) {
                result += '|';
            }
            result += label;
        }
    }

    return result.empty() ? "None" : result;
}

template <>
std::optional<VehicleFlags> fromString<VehicleFlags>(std::string_view text)
{
    for (auto [value, label] : vehicleFlagMappings()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
