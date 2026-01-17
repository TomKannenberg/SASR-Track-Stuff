#pragma once

#include <memory>
#include <vector>

#include "../../../Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"
#include "Plane3.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypoint;
class NavRacingLineRef;
class NavSpatialGroup;

namespace Serialization = SlLib::Serialization;

class NavWaypointLink final : public Serialization::IResourceSerializable
{
public:
    NavWaypointLink();
    ~NavWaypointLink();

    SlLib::Math::Vector3 FromToNormal{};
    SlLib::Math::Vector3 Right{};
    SlLib::Math::Vector3 Left{};
    SlLib::Math::Vector3 RacingLineLimitLeft{};
    SlLib::Math::Vector3 RacingLineLimitRight{};
    Plane3 Plane{};
    float RacingLineLeftScalar = 0.0f;
    float RacingLineRightScalar = 0.0f;
    NavWaypoint* From = nullptr;
    NavWaypoint* To = nullptr;
    float Length = 0.0f;
    float Width = 1.0f;
    std::vector<SlLib::Math::Vector3> CrossSection;
    std::vector<std::shared_ptr<NavRacingLineRef>> RacingLines;
    std::shared_ptr<NavSpatialGroup> SpatialGroup;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
