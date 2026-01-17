#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../../../Math/Vector.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypointLink;

class NavWaypoint
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
    int Permissions = 0;
    NavWaypoint* UnknownWaypoint = nullptr;
    std::vector<NavWaypointLink*> FromLinks;
    std::vector<NavWaypointLink*> ToLinks;
};

} // namespace SlLib::SumoTool::Siff::NavData
