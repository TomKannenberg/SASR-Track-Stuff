#include "SeDefinitionTimeLineEventBaseNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

int SeDefinitionTimeLineEventBaseNode::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    offset = SeDefinitionNode::LoadInternal(context, offset);
    Start = context.ReadFloat(0x80);
    Duration = context.ReadFloat(0x84);
    return offset + 0x10;
}

void SeDefinitionTimeLineEventBaseNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionNode::Save(context, buffer);
    context.WriteFloat(buffer, Start, 0x80);
    context.WriteFloat(buffer, Duration, 0x84);
}

int SeDefinitionTimeLineEventBaseNode::GetSizeForSerialization() const
{
    return 0x90;
}

} // namespace SlLib::Resources::Scene
