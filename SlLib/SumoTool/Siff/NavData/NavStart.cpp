#include "NavStart.hpp"

#include "NavTrackMarker.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

NavStart::NavStart() = default;
NavStart::~NavStart() = default;

void NavStart::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    DriveMode = context.ReadInt32();
    context.Position += 0x2c;
    TrackMarker = context.LoadSharedPointer<NavTrackMarker>();
}

void NavStart::Save(SlLib::Serialization::ResourceSaveContext&,
                    SlLib::Serialization::ISaveBuffer&)
{
}

int NavStart::GetSizeForSerialization() const
{
    return 0x40;
}

} // namespace SlLib::SumoTool::Siff::NavData
