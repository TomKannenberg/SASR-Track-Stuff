#include "NavWaypoint.hpp"

#include "NavTrackMarker.hpp"
#include "NavWaypointLink.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <algorithm>
#include <iostream>

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

NavWaypoint::NavWaypoint() = default;
NavWaypoint::~NavWaypoint() = default;

void NavWaypoint::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Pos = ReadAlignedFloat3(context);
    Dir = ReadAlignedFloat3(context);
    Up = ReadAlignedFloat3(context);
    Name = ReadFixedString(context, 0x40);

    TrackDist = context.ReadFloat();
    Flags = context.ReadInt32();

    int numToLinks = context.ReadInt32();
    int numFromLinks = context.ReadInt32();

    ToLinks = context.LoadPointerArray<NavWaypointLink>(numToLinks);
    FromLinks = context.LoadArrayPointer<NavWaypointLink>(numFromLinks);

    for (auto const& link : ToLinks)
    {
        if (link)
            link->From = this;
    }
    for (auto const& link : FromLinks)
    {
        if (link)
            link->To = this;
    }

    TargetSpeed = context.ReadFloat();

    if (context.Version < 0x9)
    {
        if (context.ReadPointer() != 0)
            std::cerr << "[Navigation] NavWaypoint SH sample set not supported." << std::endl;

        int trackData = context.ReadPointer();
        int numMarkers = context.ReadInt32();

        TrackMarkers.clear();
        if (trackData != 0 && numMarkers > 0)
        {
            std::size_t link = context.Position;
            context.Position = static_cast<std::size_t>(trackData);
            TrackMarkers.reserve(static_cast<std::size_t>(numMarkers));
            for (int i = 0; i < numMarkers; ++i)
                TrackMarkers.push_back(context.LoadSharedPointer<NavTrackMarker>());
            context.Position = link;
        }

        SnowLevel = context.ReadFloat();

        for (std::size_t i = 0; i < FogBlend.size(); ++i)
            FogBlend[i] = static_cast<std::uint8_t>(context.ReadInt8());
        for (std::size_t i = 0; i < BloomBlend.size(); ++i)
            BloomBlend[i] = static_cast<std::uint8_t>(context.ReadInt8());
        for (std::size_t i = 0; i < ExposureBlend.size(); ++i)
            ExposureBlend[i] = static_cast<std::uint8_t>(context.ReadInt8());
    }
    else
    {
        int trackData = context.ReadPointer();
        int numMarkers = context.ReadInt32();
        (void)trackData;
        (void)numMarkers;

        context.Position += 8;
        Permissions = context.ReadInt32();
        UnknownWaypoint = context.LoadSharedPointer<NavWaypoint>();
    }
}

void NavWaypoint::Save(SlLib::Serialization::ResourceSaveContext&,
                       SlLib::Serialization::ISaveBuffer&)
{
}

int NavWaypoint::GetSizeForSerialization() const
{
    return 0xb0;
}

} // namespace SlLib::SumoTool::Siff::NavData
