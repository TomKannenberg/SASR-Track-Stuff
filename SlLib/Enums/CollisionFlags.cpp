#include "CollisionFlags.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto collisionFlagLabels()
{
    return std::array{
        std::pair{CollisionFlags::Land, "Land"},
        std::pair{CollisionFlags::Wall, "Wall"},
    };
}

} // namespace

std::string toString(CollisionFlags flags)
{
    if (flags == CollisionFlags::None) {
        return "None";
    }

    std::string result;
    for (auto [value, label] : collisionFlagLabels()) {
        if ((flags & value) != CollisionFlags::None) {
            if (!result.empty()) {
                result += '|';
            }
            result += label;
        }
    }

    return result.empty() ? "None" : result;
}

template <>
std::optional<CollisionFlags> fromString<CollisionFlags>(std::string_view text)
{
    for (auto [value, label] : collisionFlagLabels()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
