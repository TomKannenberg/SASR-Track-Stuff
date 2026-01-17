#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class WeaponPodMessage : std::int32_t
{
    AllStar = -1325069747,
    Empty = 279773099,
    DefaultRandom = -665002936,
    Homing_Land = -1340887950,
    Homing_Water = 38432161,
    Homing_Air = -1340887950,
    Boost_Land = 649688170,
    Boost_Water = 649688170,
    Boost_Air = 649688170,
    Boost_x2_Land = 649688170,
    Boost_x2_Water = 649688170,
    Boost_x2_Air = 649688170,
    Blast_Land = 1322294921,
    Blast_Water = 1322294921,
    Blast_Air = 1322294921,
    Cannon_Land = -1878728231,
    Cannon_Water = -1078352079,
    Cannon_Air = 1376752554,
    Mine_Land = -1686414982,
    Mine_Water = -657838474,
    Mine_Air = -2021948558,
    Mine_x2_Land = -1686414982,
    Mine_x2_Water = -657838474,
    Mine_x2_Air = -2021948558,
    Shield_Land = 1249118685,
    Shield_Water = 1249118685,
    Shield_Air = 1249118685,
    Warp_Land = -1628212777,
    Warp_Water = -1628212777,
    Warp_Air = -1628212777,
    FirstPlace_Land = 1376104523,
    FirstPlace_Water = 1376104523,
    FirstPlace_Air = 1376104523,
    Flippy_Land = 604365258,
    Flippy_Water = 604365258,
    Flippy_Air = 604365258,
    RCVehicle_Land = 1676188095,
    RCVehicle_Water = 1676188095,
    RCVehicle_Air = 1676188095,
    RCVehicle_x2_Land = 1676188095,
    RCVehicle_x2_Water = 1676188095,
    RCVehicle_x2_Air = 1676188095,
    SuperGlove_Land = -2095528812,
    SuperGlove_Water = -2095528812,
    SuperGlove_Air = -2095528812,
    Firework_Land = 165142023,
    Firework_Water = 165142023,
    Firework_Air = 165142023,
    AllStarMedal = 1423458607,
    Pursuit = -1540137815,
    Time = 1611855546,
};

inline constexpr auto allWeaponPodMessages() noexcept -> std::array<WeaponPodMessage, 51>
{
    return {WeaponPodMessage::AllStar,
            WeaponPodMessage::Empty,
            WeaponPodMessage::DefaultRandom,
            WeaponPodMessage::Homing_Land,
            WeaponPodMessage::Homing_Water,
            WeaponPodMessage::Homing_Air,
            WeaponPodMessage::Boost_Land,
            WeaponPodMessage::Boost_Water,
            WeaponPodMessage::Boost_Air,
            WeaponPodMessage::Boost_x2_Land,
            WeaponPodMessage::Boost_x2_Water,
            WeaponPodMessage::Boost_x2_Air,
            WeaponPodMessage::Blast_Land,
            WeaponPodMessage::Blast_Water,
            WeaponPodMessage::Blast_Air,
            WeaponPodMessage::Cannon_Land,
            WeaponPodMessage::Cannon_Water,
            WeaponPodMessage::Cannon_Air,
            WeaponPodMessage::Mine_Land,
            WeaponPodMessage::Mine_Water,
            WeaponPodMessage::Mine_Air,
            WeaponPodMessage::Mine_x2_Land,
            WeaponPodMessage::Mine_x2_Water,
            WeaponPodMessage::Mine_x2_Air,
            WeaponPodMessage::Shield_Land,
            WeaponPodMessage::Shield_Water,
            WeaponPodMessage::Shield_Air,
            WeaponPodMessage::Warp_Land,
            WeaponPodMessage::Warp_Water,
            WeaponPodMessage::Warp_Air,
            WeaponPodMessage::FirstPlace_Land,
            WeaponPodMessage::FirstPlace_Water,
            WeaponPodMessage::FirstPlace_Air,
            WeaponPodMessage::Flippy_Land,
            WeaponPodMessage::Flippy_Water,
            WeaponPodMessage::Flippy_Air,
            WeaponPodMessage::RCVehicle_Land,
            WeaponPodMessage::RCVehicle_Water,
            WeaponPodMessage::RCVehicle_Air,
            WeaponPodMessage::RCVehicle_x2_Land,
            WeaponPodMessage::RCVehicle_x2_Water,
            WeaponPodMessage::RCVehicle_x2_Air,
            WeaponPodMessage::SuperGlove_Land,
            WeaponPodMessage::SuperGlove_Water,
            WeaponPodMessage::SuperGlove_Air,
            WeaponPodMessage::Firework_Land,
            WeaponPodMessage::Firework_Water,
            WeaponPodMessage::Firework_Air,
            WeaponPodMessage::AllStarMedal,
            WeaponPodMessage::Pursuit,
            WeaponPodMessage::Time};
}

std::string toString(WeaponPodMessage message);

template <>
std::optional<WeaponPodMessage> fromString<WeaponPodMessage>(std::string_view text);

inline std::optional<WeaponPodMessage> fromStringWeaponPodMessage(std::string_view text)
{
    return fromString<WeaponPodMessage>(text);
}

} // namespace SlLib::Enums
