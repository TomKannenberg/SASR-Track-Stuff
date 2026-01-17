#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class LeaderType
{
    None = 0,
    Leader = 1,
    LeadingHuman = 2,
    LeadingAi = 3,
    LeadingLocal = 4,
};

inline constexpr auto allLeaderTypes() noexcept -> std::array<LeaderType, 5>
{
    return {LeaderType::None, LeaderType::Leader, LeaderType::LeadingHuman, LeaderType::LeadingAi, LeaderType::LeadingLocal};
}

std::string toString(LeaderType type);

template <>
std::optional<LeaderType> fromString<LeaderType>(std::string_view text);

inline std::optional<LeaderType> fromStringLeaderType(std::string_view text)
{
    return fromString<LeaderType>(text);
}

} // namespace SlLib::Enums
