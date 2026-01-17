#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class DriveMode
{
    Car,
    Plane,
    Boat,
};

inline constexpr auto allDriveModes() noexcept -> std::array<DriveMode, 3>
{
    return {DriveMode::Car, DriveMode::Plane, DriveMode::Boat};
}

std::string toString(DriveMode mode);

template <>
std::optional<DriveMode> fromString<DriveMode>(std::string_view text);

inline std::optional<DriveMode> fromStringDriveMode(std::string_view text)
{
    return fromString<DriveMode>(text);
}

} // namespace SlLib::Enums
