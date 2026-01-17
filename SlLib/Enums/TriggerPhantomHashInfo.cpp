#include "TriggerPhantomHashInfo.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto triggerPhantomHashInfoLabels()
{
    return std::array{
        std::pair{TriggerPhantomHashInfo::Empty, "Empty"},
        std::pair{TriggerPhantomHashInfo::AirBoost, "AirBoost"},
        std::pair{TriggerPhantomHashInfo::BoostPad, "BoostPad"},
        std::pair{TriggerPhantomHashInfo::BoatBoostGate, "BoatBoostGate"},
        std::pair{TriggerPhantomHashInfo::TransformCar, "TransformCar"},
        std::pair{TriggerPhantomHashInfo::TransformPlane, "TransformPlane"},
        std::pair{TriggerPhantomHashInfo::TransformBoat, "TransformBoat"},
        std::pair{TriggerPhantomHashInfo::LandPlane, "LandPlane"},
        std::pair{TriggerPhantomHashInfo::SpinOut, "SpinOut"},
        std::pair{TriggerPhantomHashInfo::SpinOutHeavy, "SpinOutHeavy"},
        std::pair{TriggerPhantomHashInfo::SecondaryWeaponEffect, "SecondaryWeaponEffect"},
        std::pair{TriggerPhantomHashInfo::Respot, "Respot"},
        std::pair{TriggerPhantomHashInfo::MineExplosionFX, "MineExplosionFX"},
        std::pair{TriggerPhantomHashInfo::Explosion, "Explosion"},
        std::pair{TriggerPhantomHashInfo::Squash, "Squash"},
        std::pair{TriggerPhantomHashInfo::SquashHeavy, "SquashHeavy"},
        std::pair{TriggerPhantomHashInfo::SlowdownShunt, "SlowdownShunt"},
        std::pair{TriggerPhantomHashInfo::SlowdownShuntLight, "SlowdownShuntLight"},
        std::pair{TriggerPhantomHashInfo::FillWater, "FillWater"},
        std::pair{TriggerPhantomHashInfo::HideNode, "HideNode"},
        std::pair{TriggerPhantomHashInfo::ShowNode, "ShowNode"},
        std::pair{TriggerPhantomHashInfo::EnableNode, "EnableNode"},
        std::pair{TriggerPhantomHashInfo::DisableNode, "DisableNode"},
        std::pair{TriggerPhantomHashInfo::ForceTransformCar, "ForceTransformCar"},
        std::pair{TriggerPhantomHashInfo::PlayAudio, "PlayAudio"},
        std::pair{TriggerPhantomHashInfo::FadeAudio, "FadeAudio"},
        std::pair{TriggerPhantomHashInfo::PlayAnimationLink, "PlayAnimationLink"},
        std::pair{TriggerPhantomHashInfo::CameraShake, "CameraShake"},
        std::pair{TriggerPhantomHashInfo::TriggerParticleLink, "TriggerParticleLink"},
        std::pair{TriggerPhantomHashInfo::ScreenSplash, "ScreenSplash"},
        std::pair{TriggerPhantomHashInfo::CollectAnObject, "CollectAnObject"},
        std::pair{TriggerPhantomHashInfo::DropAnObject, "DropAnObject"},
        std::pair{TriggerPhantomHashInfo::Teleport, "Teleport"},
        std::pair{TriggerPhantomHashInfo::TimedTeleport, "TimedTeleport"},
        std::pair{TriggerPhantomHashInfo::TargetSmash, "TargetSmash"},
        std::pair{TriggerPhantomHashInfo::SwitchToCarRoute, "SwitchToCarRoute"},
        std::pair{TriggerPhantomHashInfo::SwitchToBoatRoute, "SwitchToBoatRoute"},
        std::pair{TriggerPhantomHashInfo::SwitchToPlaneRoute, "SwitchToPlaneRoute"},
        std::pair{TriggerPhantomHashInfo::SwitchToExclusiveCarRoute, "SwitchToExclusiveCarRoute"},
        std::pair{TriggerPhantomHashInfo::SwitchToExclusiveBoatRoute, "SwitchToExclusiveBoatRoute"},
        std::pair{TriggerPhantomHashInfo::SwitchToExclusivePlaneRoute, "SwitchToExclusivePlaneRoute"},
        std::pair{TriggerPhantomHashInfo::AiObstacle, "AiObstacle"},
        std::pair{TriggerPhantomHashInfo::AiGenericTarget, "AiGenericTarget"},
        std::pair{TriggerPhantomHashInfo::EnableRacingLines, "EnableRacingLines"},
        std::pair{TriggerPhantomHashInfo::DisableRacingLines, "DisableRacingLines"},
        std::pair{TriggerPhantomHashInfo::EnableRacingLinesPermanently, "EnableRacingLinesPermanently"},
        std::pair{TriggerPhantomHashInfo::DisableRacingLinesPermanently, "DisableRacingLinesPermanently"},
        std::pair{TriggerPhantomHashInfo::SwitchToOpenRoute, "SwitchToOpenRoute"},
        std::pair{TriggerPhantomHashInfo::TransformCarPrompt, "TransformCarPrompt"},
        std::pair{TriggerPhantomHashInfo::TransformBoatPrompt, "TransformBoatPrompt"},
        std::pair{TriggerPhantomHashInfo::TransformPlanePrompt, "TransformPlanePrompt"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_SmallObject_Pan, "AudioSwoosh_SmallObject_Pan"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_MediumObject_Pan, "AudioSwoosh_MediumObject_Pan"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_LargeObject_Pan, "AudioSwoosh_LargeObject_Pan"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_SmallObject_Center, "AudioSwoosh_SmallObject_Center"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_MediumObject_Center, "AudioSwoosh_MediumObject_Center"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_LargeObject_Center, "AudioSwoosh_LargeObject_Center"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_EnterTunnel, "AudioSwoosh_EnterTunnel"},
        std::pair{TriggerPhantomHashInfo::AudioSwoosh_ExitTunnel, "AudioSwoosh_ExitTunnel"},
        std::pair{TriggerPhantomHashInfo::Cameo_EnableSplineMovement, "Cameo_EnableSplineMovement"},
        std::pair{TriggerPhantomHashInfo::EnableTag, "EnableTag"},
        std::pair{TriggerPhantomHashInfo::DisableTag, "DisableTag"},
        std::pair{TriggerPhantomHashInfo::TransformRespot, "TransformRespot"},
        std::pair{TriggerPhantomHashInfo::NightsRing, "NightsRing"},
        std::pair{TriggerPhantomHashInfo::NightsRingFinal, "NightsRingFinal"},
        std::pair{TriggerPhantomHashInfo::ForceLap1, "ForceLap1"},
        std::pair{TriggerPhantomHashInfo::ForceLap2, "ForceLap2"},
        std::pair{TriggerPhantomHashInfo::ForceLap3, "ForceLap3"},
        std::pair{TriggerPhantomHashInfo::TriggerNode, "TriggerNode"},
        std::pair{TriggerPhantomHashInfo::Electrify_LightSlowdown, "Electrify_LightSlowdown"},
        std::pair{TriggerPhantomHashInfo::Electrify_HeavySlowdown, "Electrify_HeavySlowdown"},
        std::pair{TriggerPhantomHashInfo::TriggerLightSet1, "TriggerLightSet1"},
        std::pair{TriggerPhantomHashInfo::TriggerLightSet2, "TriggerLightSet2"},
        std::pair{TriggerPhantomHashInfo::TriggerLightSet3, "TriggerLightSet3"},
        std::pair{TriggerPhantomHashInfo::EnableForRacerViewport, "EnableForRacerViewport"},
        std::pair{TriggerPhantomHashInfo::DisableForRacerViewport, "DisableForRacerViewport"},
        std::pair{TriggerPhantomHashInfo::ForceTriggerPhantom, "ForceTriggerPhantom"},
    };
}

} // namespace

std::string toString(TriggerPhantomHashInfo hashInfo)
{
    for (auto [value, label] : triggerPhantomHashInfoLabels()) {
        if (value == hashInfo) {
            return std::string{label};
        }
    }

    return "Unknown";
}

template <>
std::optional<TriggerPhantomHashInfo> fromString<TriggerPhantomHashInfo>(std::string_view text)
{
    for (auto [value, label] : triggerPhantomHashInfoLabels()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
