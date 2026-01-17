#pragma once

#include <memory>
#include <string>

#include "../../../Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypoint;

namespace Serialization = SlLib::Serialization;

class NavTrackMarker final : public Serialization::IResourceSerializable
{
public:
    NavTrackMarker();
    ~NavTrackMarker();

    SlLib::Math::Vector3 Pos{};
    SlLib::Math::Vector3 Dir{0.0f, 0.0f, 1.0f};
    SlLib::Math::Vector3 Up{0.0f, 1.0f, 0.0f};
    int Type = 0;
    float Radius = 20.0f;
    std::shared_ptr<NavWaypoint> Waypoint;
    int Value = 0;
    float TrackDist = 0.0f;
    std::shared_ptr<NavTrackMarker> LinkedTrackMarker;
    float JumpSpeedPercentage = 0.65f;
    int Flags = 0;
    std::string Text;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
