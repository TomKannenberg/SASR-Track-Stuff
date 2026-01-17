#include "SeDefinitionTimeLineFlowControlEvent.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

int SeDefinitionTimeLineFlowControlEvent::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    offset = SeDefinitionTimeLineEventBaseNode::LoadInternal(context, offset);
    AutoLoopCountReset = context.ReadBoolean(0x90);
    LoopCount = context.ReadInt32(0x94);
    return offset + 0x10;
}

void SeDefinitionTimeLineFlowControlEvent::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTimeLineEventBaseNode::Save(context, buffer);
    context.WriteBoolean(buffer, AutoLoopCountReset, 0x90);
    context.WriteInt32(buffer, LoopCount, 0x94);
}

int SeDefinitionTimeLineFlowControlEvent::GetSizeForSerialization() const
{
    return 0x98;
}

} // namespace SlLib::Resources::Scene
