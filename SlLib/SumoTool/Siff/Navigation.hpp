#pragma once

#include <memory>
#include <string>
#include <vector>

#include "NavData/NavRacingLine.hpp"
#include "NavData/NavWaypoint.hpp"
#include "NavData/NavWaypointLink.hpp"

namespace SlLib::SumoTool::Siff {

class Navigation
{
public:
    Navigation();
    ~Navigation();

    void GenerateTestRoute();

    std::vector<std::unique_ptr<NavData::NavWaypoint>> Waypoints;
    std::vector<std::unique_ptr<NavData::NavWaypointLink>> Links;
    std::vector<NavData::NavRacingLine> RacingLines;
};

} // namespace SlLib::SumoTool::Siff
