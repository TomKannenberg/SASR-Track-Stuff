#include "TriggerPhantomDefinitionNode.hpp"

#include "SlLib/Serialization/ISaveBuffer.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

void TriggerPhantomDefinitionNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionNode::Load(context);

    int pos = context.Version <= 0xb ? 0x10 : 0x0;
    WidthRadius = context.ReadFloat(pos + 0xd0);
    Height = context.ReadFloat(pos + 0xd4);
    Depth = context.ReadFloat(pos + 0xd8);
    Shape = context.ReadInt32(pos + 0xdc);
    SendChildMessages = context.ReadInt32(pos + 0xe0);
}

void TriggerPhantomDefinitionNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);
    context.WriteFloat(buffer, WidthRadius, 0xd0);
    context.WriteFloat(buffer, Height, 0xd4);
    context.WriteFloat(buffer, Depth, 0xd8);
    context.WriteInt32(buffer, Shape, 0xdc);
    context.WriteInt32(buffer, SendChildMessages, 0xe0);
}

int TriggerPhantomDefinitionNode::GetSizeForSerialization() const
{
    return 0xf0;
}

} // namespace SlLib::Resources::Scene
