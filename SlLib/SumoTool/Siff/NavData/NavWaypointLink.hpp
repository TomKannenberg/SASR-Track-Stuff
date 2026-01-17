#pragma once

#include <vector>

#include "../../../Math/Vector.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypoint;

class NavWaypointLink
{
public:
    NavWaypointLink();
    ~NavWaypointLink();

    SlLib::Math::Vector3 FromToNormal{};
    SlLib::Math::Vector3 Right{};
    SlLib::Math::Vector3 Left{};
    SlLib::Math::Vector3 RacingLineLimitLeft{};
    SlLib::Math::Vector3 RacingLineLimitRight{};
    SlLib::Math::Vector3 PlaneNormal{};
    float RacingLineLeftScalar = 0.0f;
    float RacingLineRightScalar = 0.0f;
    NavWaypoint* From = nullptr;
    NavWaypoint* To = nullptr;
    float Length = 0.0f;
    float Width = 1.0f;
    std::vector<SlLib::Math::Vector3> CrossSection;
};

} // namespace SlLib::SumoTool::Siff::NavData
