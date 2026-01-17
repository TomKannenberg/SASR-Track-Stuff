#include "NavRacingLineSeg.hpp"

#include "NavWaypointLink.hpp"
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

} // namespace

NavRacingLineSeg::NavRacingLineSeg() = default;
NavRacingLineSeg::~NavRacingLineSeg() = default;

void NavRacingLineSeg::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    RacingLine = ReadAlignedFloat3(context);
    SafeRacingLine = ReadAlignedFloat3(context);
    RacingLineScalar = context.ReadFloat();
    SafeRacingLineScalar = context.ReadFloat();
    Link = context.LoadSharedPointer<NavWaypointLink>();
    RacingLineLength = context.ReadFloat();
    TurnType = context.ReadInt32();
    SmoothSideLeft = context.ReadFloat();
    SmoothSideRight = context.ReadFloat();
}

void NavRacingLineSeg::Save(SlLib::Serialization::ResourceSaveContext&,
                            SlLib::Serialization::ISaveBuffer&)
{
}

int NavRacingLineSeg::GetSizeForSerialization() const
{
    return 0x40;
}

} // namespace SlLib::SumoTool::Siff::NavData
