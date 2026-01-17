#include "DriveMode.hpp"

namespace SlLib::Enums {

std::string toString(DriveMode mode)
{
    switch (mode) {
    case DriveMode::Car:
        return "Car";
    case DriveMode::Plane:
        return "Plane";
    case DriveMode::Boat:
        return "Boat";
    }

    return "Unknown";
}

template <>
std::optional<DriveMode> fromString<DriveMode>(std::string_view text)
{
    return fromStringFromAll<DriveMode>(allDriveModes, text);
}

} // namespace SlLib::Enums
