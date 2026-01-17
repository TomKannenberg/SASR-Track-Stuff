#pragma once

#include "../../../Math/Vector.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypointLink;

class NavRacingLineSeg
{
public:
    NavRacingLineSeg();
    ~NavRacingLineSeg();

    SlLib::Math::Vector3 RacingLine{};
    NavWaypointLink* Link = nullptr;
};

} // namespace SlLib::SumoTool::Siff::NavData
