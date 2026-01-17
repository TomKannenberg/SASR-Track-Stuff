#include "Trigger.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::Logic {

Trigger::Trigger() = default;
Trigger::~Trigger() = default;

void Trigger::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    NameHash = context.ReadInt32();
    NumAttributes = context.ReadInt32();
    AttributeStartIndex = context.ReadInt32();
    Flags = context.ReadInt32();
    Position = context.ReadFloat4();
    Normal = context.ReadFloat4();
    Vertex0 = context.ReadFloat4();
    Vertex1 = context.ReadFloat4();
    Vertex2 = context.ReadFloat4();
    Vertex3 = context.ReadFloat4();
}

void Trigger::Save(SlLib::Serialization::ResourceSaveContext&,
                   SlLib::Serialization::ISaveBuffer&)
{
}

int Trigger::GetSizeForSerialization() const
{
    return 0x70;
}

} // namespace SlLib::SumoTool::Siff::Logic
