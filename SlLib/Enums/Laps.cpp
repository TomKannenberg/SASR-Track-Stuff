#include "Laps.hpp"

#include <string_view>

namespace SlLib::Enums {

std::string toString(Laps lap)
{
    switch (lap) {
    case Laps::Lap1:
        return "Lap1";
    case Laps::Lap2:
        return "Lap2";
    case Laps::Lap3:
        return "Lap3";
    case Laps::Lap4:
        return "Lap4";
    }

    return "Unknown";
}

template <>
std::optional<Laps> fromString<Laps>(std::string_view text)
{
    return fromStringFromAll<Laps>(allLaps, text);
}

} // namespace SlLib::Enums
