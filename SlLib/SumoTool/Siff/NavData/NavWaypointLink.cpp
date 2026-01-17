#include "NavWaypointLink.hpp"

#include "NavRacingLineRef.hpp"
#include "NavSpatialGroup.hpp"
#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

namespace {

SlLib::Math::Vector3 ReadAlignedFloat3(SlLib::Serialization::ResourceLoadContext& context)
{
    auto value = context.ReadFloat3();
    context.Position += 4;
    return value;
}

int PointerSize(SlLib::Serialization::ResourceLoadContext const& context)
{
    return context.Platform && context.Platform->Is64Bit() ? 8 : 4;
}

} // namespace

NavWaypointLink::NavWaypointLink() = default;
NavWaypointLink::~NavWaypointLink() = default;

void NavWaypointLink::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    FromToNormal = ReadAlignedFloat3(context);
    Right = ReadAlignedFloat3(context);
    Left = ReadAlignedFloat3(context);
    RacingLineLimitLeft = ReadAlignedFloat3(context);
    RacingLineLimitRight = ReadAlignedFloat3(context);

    Plane.Normal = ReadAlignedFloat3(context);
    Plane.Const = context.ReadFloat();
    context.Position += 0x0c;

    RacingLineLeftScalar = context.ReadFloat();
    RacingLineRightScalar = context.ReadFloat();

    context.Position += PointerSize(context) * 2;

    Length = context.ReadFloat();
    Width = context.ReadFloat();

    int crossSectionCount = context.ReadInt32();
    CrossSection = context.LoadArrayPointer(crossSectionCount, ReadAlignedFloat3);

    context.ReadPointer();
    int racingLineData = context.ReadPointer();
    int racingLineCount = context.ReadInt32();
    RacingLines.clear();
    if (racingLineData != 0 && racingLineCount > 0)
    {
        std::size_t link = context.Position;
        context.Position = static_cast<std::size_t>(racingLineData);
        RacingLines.reserve(static_cast<std::size_t>(racingLineCount));
        for (int i = 0; i < racingLineCount; ++i)
            RacingLines.push_back(context.LoadSharedReference<NavRacingLineRef>());
        context.Position = link;
    }

    SpatialGroup = context.LoadSharedPointer<NavSpatialGroup>();
}

void NavWaypointLink::Save(SlLib::Serialization::ResourceSaveContext&,
                           SlLib::Serialization::ISaveBuffer&)
{
}

int NavWaypointLink::GetSizeForSerialization() const
{
    return 0xa0;
}

} // namespace SlLib::SumoTool::Siff::NavData
