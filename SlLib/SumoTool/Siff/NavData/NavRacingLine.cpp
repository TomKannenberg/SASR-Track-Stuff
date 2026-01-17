#include "NavRacingLine.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

NavRacingLine::NavRacingLine() = default;
NavRacingLine::~NavRacingLine() = default;

void NavRacingLine::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int segmentCount = context.ReadInt32();
    Segments = context.LoadArrayPointer<NavRacingLineSeg>(segmentCount);
    Looping = context.ReadBoolean(true);
    Permissions = context.ReadInt32();
    TotalLength = context.ReadFloat();
    TotalBaseTime = context.ReadFloat();
}

void NavRacingLine::Save(SlLib::Serialization::ResourceSaveContext&,
                         SlLib::Serialization::ISaveBuffer&)
{
}

int NavRacingLine::GetSizeForSerialization() const
{
    return 0x20;
}

} // namespace SlLib::SumoTool::Siff::NavData
