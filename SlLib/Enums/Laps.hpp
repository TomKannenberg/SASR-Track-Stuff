#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class Laps
{
    Lap1,
    Lap2,
    Lap3,
    Lap4,
};

inline constexpr auto allLaps() noexcept -> std::array<Laps, 4>
{
    return {Laps::Lap1, Laps::Lap2, Laps::Lap3, Laps::Lap4};
}

std::string toString(Laps lap);

template <>
std::optional<Laps> fromString<Laps>(std::string_view text);

inline std::optional<Laps> fromStringLaps(std::string_view text)
{
    return fromString<Laps>(text);
}

} // namespace SlLib::Enums
