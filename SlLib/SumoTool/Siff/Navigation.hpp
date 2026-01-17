#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "SlLib/Serialization/IResourceSerializable.hpp"
#include "NavData/NavRacingLine.hpp"
#include "NavData/NavSpatialGroup.hpp"
#include "NavData/NavStart.hpp"
#include "NavData/NavTrackMarker.hpp"
#include "NavData/NavWaypoint.hpp"
#include "NavData/NavWaypointLink.hpp"

namespace SlLib::SumoTool::Siff {

namespace Serialization = SlLib::Serialization;

class Navigation final : public Serialization::IResourceSerializable
{
public:
    Navigation();
    ~Navigation();

    void GenerateTestRoute();

    int NameHash = 0;
    int Version = 0;
    bool Remapped = false;
    std::vector<SlLib::Math::Vector3> Vertices;
    std::vector<SlLib::Math::Vector3> Normals;
    std::vector<std::shared_ptr<NavData::NavWaypoint>> Waypoints;
    std::vector<std::shared_ptr<NavData::NavWaypointLink>> Links;
    std::vector<std::shared_ptr<NavData::NavRacingLine>> RacingLines;
    std::vector<std::shared_ptr<NavData::NavTrackMarker>> TrackMarkers;
    std::vector<std::shared_ptr<NavData::NavSpatialGroup>> SpatialGroups;
    std::vector<std::shared_ptr<NavData::NavStart>> NavStarts;
    float TotalTrackDist = 0.0f;
    float LowestPoint = 0.0f;
    float TrackBottomLeftX = 0.0f;
    float TrackBottomLeftZ = 0.0f;
    float TrackTopRightX = 0.0f;
    float TrackTopRightZ = 0.0f;
    float HighestPoint = 0.0f;
    std::array<int, 4> SettingsFogNameHashes{};
    std::array<int, 4> SettingsBloomNameHashes{};
    std::array<int, 4> SettingsExposureNameHashes{};

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff
