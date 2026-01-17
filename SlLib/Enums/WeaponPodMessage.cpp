#include "WeaponPodMessage.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto weaponPodMessageLabels()
{
    return std::array{
        std::pair{WeaponPodMessage::AllStar, "AllStar"},
        std::pair{WeaponPodMessage::Empty, "Empty"},
        std::pair{WeaponPodMessage::DefaultRandom, "DefaultRandom"},
        std::pair{WeaponPodMessage::Homing_Land, "Homing_Land"},
        std::pair{WeaponPodMessage::Homing_Water, "Homing_Water"},
        std::pair{WeaponPodMessage::Homing_Air, "Homing_Air"},
        std::pair{WeaponPodMessage::Boost_Land, "Boost_Land"},
        std::pair{WeaponPodMessage::Boost_Water, "Boost_Water"},
        std::pair{WeaponPodMessage::Boost_Air, "Boost_Air"},
        std::pair{WeaponPodMessage::Boost_x2_Land, "Boost_x2_Land"},
        std::pair{WeaponPodMessage::Boost_x2_Water, "Boost_x2_Water"},
        std::pair{WeaponPodMessage::Boost_x2_Air, "Boost_x2_Air"},
        std::pair{WeaponPodMessage::Blast_Land, "Blast_Land"},
        std::pair{WeaponPodMessage::Blast_Water, "Blast_Water"},
        std::pair{WeaponPodMessage::Blast_Air, "Blast_Air"},
        std::pair{WeaponPodMessage::Cannon_Land, "Cannon_Land"},
        std::pair{WeaponPodMessage::Cannon_Water, "Cannon_Water"},
        std::pair{WeaponPodMessage::Cannon_Air, "Cannon_Air"},
        std::pair{WeaponPodMessage::Mine_Land, "Mine_Land"},
        std::pair{WeaponPodMessage::Mine_Water, "Mine_Water"},
        std::pair{WeaponPodMessage::Mine_Air, "Mine_Air"},
        std::pair{WeaponPodMessage::Mine_x2_Land, "Mine_x2_Land"},
        std::pair{WeaponPodMessage::Mine_x2_Water, "Mine_x2_Water"},
        std::pair{WeaponPodMessage::Mine_x2_Air, "Mine_x2_Air"},
        std::pair{WeaponPodMessage::Shield_Land, "Shield_Land"},
        std::pair{WeaponPodMessage::Shield_Water, "Shield_Water"},
        std::pair{WeaponPodMessage::Shield_Air, "Shield_Air"},
        std::pair{WeaponPodMessage::Warp_Land, "Warp_Land"},
        std::pair{WeaponPodMessage::Warp_Water, "Warp_Water"},
        std::pair{WeaponPodMessage::Warp_Air, "Warp_Air"},
        std::pair{WeaponPodMessage::FirstPlace_Land, "FirstPlace_Land"},
        std::pair{WeaponPodMessage::FirstPlace_Water, "FirstPlace_Water"},
        std::pair{WeaponPodMessage::FirstPlace_Air, "FirstPlace_Air"},
        std::pair{WeaponPodMessage::Flippy_Land, "Flippy_Land"},
        std::pair{WeaponPodMessage::Flippy_Water, "Flippy_Water"},
        std::pair{WeaponPodMessage::Flippy_Air, "Flippy_Air"},
        std::pair{WeaponPodMessage::RCVehicle_Land, "RCVehicle_Land"},
        std::pair{WeaponPodMessage::RCVehicle_Water, "RCVehicle_Water"},
        std::pair{WeaponPodMessage::RCVehicle_Air, "RCVehicle_Air"},
        std::pair{WeaponPodMessage::RCVehicle_x2_Land, "RCVehicle_x2_Land"},
        std::pair{WeaponPodMessage::RCVehicle_x2_Water, "RCVehicle_x2_Water"},
        std::pair{WeaponPodMessage::RCVehicle_x2_Air, "RCVehicle_x2_Air"},
        std::pair{WeaponPodMessage::SuperGlove_Land, "SuperGlove_Land"},
        std::pair{WeaponPodMessage::SuperGlove_Water, "SuperGlove_Water"},
        std::pair{WeaponPodMessage::SuperGlove_Air, "SuperGlove_Air"},
        std::pair{WeaponPodMessage::Firework_Land, "Firework_Land"},
        std::pair{WeaponPodMessage::Firework_Water, "Firework_Water"},
        std::pair{WeaponPodMessage::Firework_Air, "Firework_Air"},
        std::pair{WeaponPodMessage::AllStarMedal, "AllStarMedal"},
        std::pair{WeaponPodMessage::Pursuit, "Pursuit"},
        std::pair{WeaponPodMessage::Time, "Time"},
    };
}

} // namespace

std::string toString(WeaponPodMessage message)
{
    for (auto [value, label] : weaponPodMessageLabels()) {
        if (value == message) {
            return std::string{label};
        }
    }

    return "Unknown";
}

template <>
std::optional<WeaponPodMessage> fromString<WeaponPodMessage>(std::string_view text)
{
    for (auto [value, label] : weaponPodMessageLabels()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
