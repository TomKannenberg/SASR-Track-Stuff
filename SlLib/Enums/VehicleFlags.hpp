#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class VehicleFlags : std::uint32_t
{
    None = 0,
    TriggerCar = 1 << 0,
    TriggerBoat = 1 << 1,
    TriggerPlane = 1 << 2,
    TriggerWeapon = 1 << 3,
    TriggerOnLoad = 1 << 5,
    TriggerPrediction = 1 << 6,
    TriggerHasJumboMap = 1 << 8,
    TriggerOncePerRacer = 1 << 9,
};

constexpr VehicleFlags operator|(VehicleFlags lhs, VehicleFlags rhs) noexcept
{
    return static_cast<VehicleFlags>(static_cast<std::uint32_t>(lhs) | static_cast<std::uint32_t>(rhs));
}

constexpr VehicleFlags operator&(VehicleFlags lhs, VehicleFlags rhs) noexcept
{
    return static_cast<VehicleFlags>(static_cast<std::uint32_t>(lhs) & static_cast<std::uint32_t>(rhs));
}

constexpr VehicleFlags operator~(VehicleFlags flags) noexcept
{
    return static_cast<VehicleFlags>(~static_cast<std::uint32_t>(flags));
}

constexpr VehicleFlags& operator|=(VehicleFlags& lhs, VehicleFlags rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

constexpr VehicleFlags& operator&=(VehicleFlags& lhs, VehicleFlags rhs) noexcept
{
    lhs = lhs & rhs;
    return lhs;
}

inline constexpr auto allVehicleFlags() noexcept -> std::array<VehicleFlags, 8>
{
    return {VehicleFlags::TriggerCar,
            VehicleFlags::TriggerBoat,
            VehicleFlags::TriggerPlane,
            VehicleFlags::TriggerWeapon,
            VehicleFlags::TriggerOnLoad,
            VehicleFlags::TriggerPrediction,
            VehicleFlags::TriggerHasJumboMap,
            VehicleFlags::TriggerOncePerRacer};
}

std::string toString(VehicleFlags flags);

template <>
std::optional<VehicleFlags> fromString<VehicleFlags>(std::string_view text);

inline std::optional<VehicleFlags> fromStringVehicleFlags(std::string_view text)
{
    return fromString<VehicleFlags>(text);
}

} // namespace SlLib::Enums
