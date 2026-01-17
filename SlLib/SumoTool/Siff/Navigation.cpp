#include "Navigation.hpp"

#include "../../Math/Vector.hpp"
#include <cstddef>
#include <iterator>

namespace SlLib::SumoTool::Siff {

Navigation::Navigation() = default;

Navigation::~Navigation() = default;

void Navigation::GenerateTestRoute()
{
    constexpr SlLib::Math::Vector3 points[] = {
        { -50.0f, 0.0f, -50.0f },
        { 50.0f, 0.0f, -50.0f },
        { 50.0f, 0.0f, 50.0f },
        { -50.0f, 0.0f, 50.0f }
    };

    for (size_t i = 0; i < std::size(points); ++i)
    {
        auto waypoint = std::make_unique<NavData::NavWaypoint>();
        waypoint->Pos = points[i];
        waypoint->Name = "waypoint_0_" + std::to_string(i);
        Waypoints.push_back(std::move(waypoint));
    }

    for (size_t i = 0; i < Waypoints.size(); ++i)
    {
        auto link = std::make_unique<NavData::NavWaypointLink>();
        auto* from = Waypoints[i].get();
        auto* to = Waypoints[(i + 1) % Waypoints.size()].get();

        link->From = from;
        link->To = to;
        link->Width = 4.0f;

        link->Left = from->Pos + SlLib::Math::Vector3{ -1.0f, 0.0f, 0.0f };
        link->Right = to->Pos + SlLib::Math::Vector3{ 1.0f, 0.0f, 0.0f };
        link->RacingLineLimitLeft = link->Left - SlLib::Math::Vector3{ 0.0f, 0.0f, 2.0f };
        link->RacingLineLimitRight = link->Right + SlLib::Math::Vector3{ 0.0f, 0.0f, 2.0f };
        link->CrossSection = { link->Left, link->Right };

        from->ToLinks.push_back(link.get());
        to->FromLinks.push_back(link.get());

        NavData::NavRacingLineSeg segment;
        segment.Link = link.get();
        segment.RacingLine = (from->Pos + to->Pos) * 0.5f;

        if (RacingLines.empty())
            RacingLines.emplace_back();

        RacingLines.front().Segments.push_back(segment);
        Links.push_back(std::move(link));
    }
}

} // namespace SlLib::SumoTool::Siff
