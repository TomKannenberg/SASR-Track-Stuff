#include "NavRacingLineRef.hpp"

#include "NavRacingLine.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

NavRacingLineRef::NavRacingLineRef() = default;
NavRacingLineRef::~NavRacingLineRef() = default;

void NavRacingLineRef::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    RacingLine = context.LoadSharedPointer<NavRacingLine>();
    SegmentIndex = context.ReadInt32();
}

void NavRacingLineRef::Save(SlLib::Serialization::ResourceSaveContext&,
                            SlLib::Serialization::ISaveBuffer&)
{
}

int NavRacingLineRef::GetSizeForSerialization() const
{
    return 0x10;
}

} // namespace SlLib::SumoTool::Siff::NavData
