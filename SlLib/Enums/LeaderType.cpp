#include "LeaderType.hpp"

namespace SlLib::Enums {

std::string toString(LeaderType type)
{
    switch (type) {
    case LeaderType::None:
        return "None";
    case LeaderType::Leader:
        return "Leader";
    case LeaderType::LeadingHuman:
        return "LeadingHuman";
    case LeaderType::LeadingAi:
        return "LeadingAi";
    case LeaderType::LeadingLocal:
        return "LeadingLocal";
    }

    return "Unknown";
}

template <>
std::optional<LeaderType> fromString<LeaderType>(std::string_view text)
{
    return fromStringFromAll<LeaderType>(allLeaderTypes, text);
}

} // namespace SlLib::Enums
