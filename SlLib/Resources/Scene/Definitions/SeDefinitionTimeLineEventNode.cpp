#include "SeDefinitionTimeLineEventNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

int SeDefinitionTimeLineEventNode::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    offset = SeDefinitionTimeLineEventBaseNode::LoadInternal(context, offset);
    StartMessageDestination = context.ReadInt32(0x90);
    StartMessage = context.ReadInt32(0x94);
    StartRecipient = static_cast<SeNodeBase*>(context.LoadNode(context.ReadInt32(0x9c)));
    EndMessageDestination = context.ReadInt32(0xb0);
    EndMessage = context.ReadInt32(0xb4);
    EndRecipient = static_cast<SeNodeBase*>(context.LoadNode(context.ReadInt32(0xbc)));
    return offset + 0x40;
}

void SeDefinitionTimeLineEventNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTimeLineEventBaseNode::Save(context, buffer);
    context.WriteInt32(buffer, StartMessageDestination, 0x90);
    context.WriteInt32(buffer, StartMessage, 0x94);
    context.WriteInt32(buffer, StartRecipient ? StartRecipient->Uid : 0, 0x9c);
    context.WriteInt32(buffer, EndMessageDestination, 0xb0);
    context.WriteInt32(buffer, EndMessage, 0xb4);
    context.WriteInt32(buffer, EndRecipient ? EndRecipient->Uid : 0, 0xbc);
}

int SeDefinitionTimeLineEventNode::GetSizeForSerialization() const
{
    return 0xd0;
}

} // namespace SlLib::Resources::Scene
