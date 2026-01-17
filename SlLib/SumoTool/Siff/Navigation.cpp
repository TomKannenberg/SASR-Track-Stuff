#include "Navigation.hpp"

#include "../../Math/Vector.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>
#include <iterator>

namespace SlLib::SumoTool::Siff {

namespace {

SlLib::Math::Vector3 ReadAlignedFloat3(SlLib::Serialization::ResourceLoadContext& context)
{
    auto value = context.ReadFloat3();
    context.Position += 4;
    return value;
}

} // namespace

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
        auto waypoint = std::make_shared<NavData::NavWaypoint>();
        waypoint->Pos = points[i];
        waypoint->Name = "waypoint_0_" + std::to_string(i);
        Waypoints.push_back(std::move(waypoint));
    }

    for (size_t i = 0; i < Waypoints.size(); ++i)
    {
        auto link = std::make_shared<NavData::NavWaypointLink>();
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

        from->ToLinks.push_back(link);
        to->FromLinks.push_back(link);

        auto segment = std::make_shared<NavData::NavRacingLineSeg>();
        segment->Link = link;
        segment->RacingLine = (from->Pos + to->Pos) * 0.5f;

        if (RacingLines.empty())
            RacingLines.push_back(std::make_shared<NavData::NavRacingLine>());

        RacingLines.front()->Segments.push_back(segment);
        Links.push_back(std::move(link));
    }
}

void Navigation::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int link = context.Base;
    context.Base = static_cast<int>(context.Position);

    NameHash = context.ReadInt32();
    Version = context.ReadInt32();
    context.Version = Version;
    Remapped = context.ReadBoolean(true);

    int numVerts = context.ReadInt32();
    int numNormals = context.ReadInt32();
    context.ReadInt32(); // numTris
    int numWaypoints = context.ReadInt32();
    int numRacingLines = context.ReadInt32();
    int numTrackMarkers = context.ReadInt32();
    context.ReadInt32(); // numTrafficRoutes
    context.ReadInt32(); // numTrafficSpawn
    context.ReadInt32(); // numBlockers
    context.ReadInt32(); // numErrors
    int numSpatialGroups = context.ReadInt32();

    int numStarts = 0;
    if (Version >= 0x9)
    {
        numStarts = context.ReadInt32();
        context.ReadInt32();
    }

    Vertices = context.LoadArrayPointer(numVerts, ReadAlignedFloat3);
    Normals = context.LoadArrayPointer(numNormals, ReadAlignedFloat3);

    context.ReadPointer(); // tris
    Waypoints = context.LoadArrayPointer<NavData::NavWaypoint>(numWaypoints);
    RacingLines = context.LoadArrayPointer<NavData::NavRacingLine>(numRacingLines);
    TrackMarkers = context.LoadArrayPointer<NavData::NavTrackMarker>(numTrackMarkers);
    context.ReadPointer(); // traffic routes
    context.ReadPointer(); // traffic spawn
    context.ReadPointer(); // blockers
    context.ReadPointer(); // errors
    SpatialGroups = context.LoadArrayPointer<NavData::NavSpatialGroup>(numSpatialGroups);
    if (Version >= 0x9)
        NavStarts = context.LoadArrayPointer<NavData::NavStart>(numStarts);

    TotalTrackDist = context.ReadFloat();
    LowestPoint = context.ReadFloat();
    TrackBottomLeftX = context.ReadFloat();
    TrackBottomLeftZ = context.ReadFloat();
    TrackTopRightX = context.ReadFloat();
    TrackTopRightZ = context.ReadFloat();
    context.ReadPointer(); // 3d racing lines
    HighestPoint = context.ReadFloat();

    for (std::size_t i = 0; i < SettingsFogNameHashes.size(); ++i)
        SettingsFogNameHashes[i] = context.ReadInt32();
    for (std::size_t i = 0; i < SettingsBloomNameHashes.size(); ++i)
        SettingsBloomNameHashes[i] = context.ReadInt32();
    for (std::size_t i = 0; i < SettingsExposureNameHashes.size(); ++i)
        SettingsExposureNameHashes[i] = context.ReadInt32();

    context.Base = link;
}

void Navigation::Save(SlLib::Serialization::ResourceSaveContext&,
                      SlLib::Serialization::ISaveBuffer&)
{
}

int Navigation::GetSizeForSerialization() const
{
    return Version > 0x8 ? 0xe0 : 0xd4;
}

} // namespace SlLib::SumoTool::Siff
