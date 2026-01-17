#include "NavTrackMarker.hpp"

#include "NavWaypoint.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <algorithm>

namespace SlLib::SumoTool::Siff::NavData {

namespace {

SlLib::Math::Vector3 ReadAlignedFloat3(SlLib::Serialization::ResourceLoadContext& context)
{
    auto value = context.ReadFloat3();
    context.Position += 4;
    return value;
}

std::string ReadFixedString(SlLib::Serialization::ResourceLoadContext& context, std::size_t size)
{
    auto data = context.Data();
    std::size_t start = context.Position;
    std::size_t end = std::min(start + size, data.size());
    std::string out;
    for (std::size_t i = start; i < end; ++i)
    {
        char c = static_cast<char>(data[i]);
        if (c == '\0')
            break;
        out.push_back(c);
    }
    context.Position = start + size;
    return out;
}

} // namespace

NavTrackMarker::NavTrackMarker() = default;
NavTrackMarker::~NavTrackMarker() = default;

void NavTrackMarker::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Pos = ReadAlignedFloat3(context);
    Dir = ReadAlignedFloat3(context);
    Up = ReadAlignedFloat3(context);
    Type = context.ReadInt32();
    Radius = context.ReadFloat();
    Waypoint = context.LoadSharedPointer<NavWaypoint>();
    Value = context.ReadInt32();
    TrackDist = context.ReadFloat();
    LinkedTrackMarker = context.LoadSharedPointer<NavTrackMarker>();
    JumpSpeedPercentage = context.ReadFloat();
    Flags = context.ReadInt32();
    Text = ReadFixedString(context, 0x10);
}

void NavTrackMarker::Save(SlLib::Serialization::ResourceSaveContext&,
                          SlLib::Serialization::ISaveBuffer&)
{
}

int NavTrackMarker::GetSizeForSerialization() const
{
    return 0x60;
}

} // namespace SlLib::SumoTool::Siff::NavData
