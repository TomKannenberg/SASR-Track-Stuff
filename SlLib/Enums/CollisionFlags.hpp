#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class CollisionFlags : std::uint32_t
{
    None = 0,
    Land = 1,
    Wall = 4,
};

constexpr CollisionFlags operator|(CollisionFlags lhs, CollisionFlags rhs) noexcept
{
    return static_cast<CollisionFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr CollisionFlags operator&(CollisionFlags lhs, CollisionFlags rhs) noexcept
{
    return static_cast<CollisionFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr CollisionFlags operator~(CollisionFlags flags) noexcept
{
    return static_cast<CollisionFlags>(~static_cast<std::uint32_t>(flags));
}

constexpr CollisionFlags& operator|=(CollisionFlags& lhs, CollisionFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr CollisionFlags& operator&=(CollisionFlags& lhs, CollisionFlags rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

std::string toString(CollisionFlags flags);

template <>
std::optional<CollisionFlags> fromString<CollisionFlags>(std::string_view text);

inline std::optional<CollisionFlags> fromStringCollisionFlags(std::string_view text)
{
    return fromString<CollisionFlags>(text);
}

} // namespace SlLib::Enums
