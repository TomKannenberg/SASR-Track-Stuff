#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "../../../Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypointLink;
class NavTrackMarker;

namespace Serialization = SlLib::Serialization;

class NavWaypoint final : public Serialization::IResourceSerializable
{
public:
    NavWaypoint();
    ~NavWaypoint();

    SlLib::Math::Vector3 Pos{};
    SlLib::Math::Vector3 Dir{0.0f, 0.0f, 1.0f};
    SlLib::Math::Vector3 Up{0.0f, 1.0f, 0.0f};
    std::string Name;
    float TrackDist = 0.0f;
    int Flags = 0;
    std::vector<std::shared_ptr<NavWaypointLink>> ToLinks;
    std::vector<std::shared_ptr<NavWaypointLink>> FromLinks;
    float TargetSpeed = 1.0f;
    std::vector<std::shared_ptr<NavTrackMarker>> TrackMarkers;
    float SnowLevel = 0.0f;
    std::array<std::uint8_t, 4> FogBlend{{0xFF, 0, 0, 0}};
    std::array<std::uint8_t, 4> BloomBlend{{0xFF, 0, 0, 0}};
    std::array<std::uint8_t, 4> ExposureBlend{{0xFF, 0, 0, 0}};
    int Permissions = 0;
    std::shared_ptr<NavWaypoint> UnknownWaypoint;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
