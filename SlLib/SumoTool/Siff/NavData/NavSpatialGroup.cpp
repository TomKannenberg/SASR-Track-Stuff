#include "NavSpatialGroup.hpp"

#include "NavWaypointLink.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

NavSpatialGroup::NavSpatialGroup() = default;
NavSpatialGroup::~NavSpatialGroup() = default;

void NavSpatialGroup::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int numLinks = context.ReadInt32();
    int linksData = context.ReadPointer();
    Links.clear();
    if (linksData != 0 && numLinks > 0)
    {
        std::size_t link = context.Position;
        context.Position = static_cast<std::size_t>(linksData);
        Links.reserve(static_cast<std::size_t>(numLinks));
        for (int i = 0; i < numLinks; ++i)
            Links.push_back(context.LoadSharedPointer<NavWaypointLink>());
        context.Position = link;
    }
}

void NavSpatialGroup::Save(SlLib::Serialization::ResourceSaveContext&,
                           SlLib::Serialization::ISaveBuffer&)
{
}

int NavSpatialGroup::GetSizeForSerialization() const
{
    return 0x10;
}

} // namespace SlLib::SumoTool::Siff::NavData
