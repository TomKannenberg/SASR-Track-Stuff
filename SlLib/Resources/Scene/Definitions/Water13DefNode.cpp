#include "Water13DefNode.hpp"

#include "SlLib/Serialization/ISaveBuffer.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

void Water13DefNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionNode::Load(context);
    WaterSimulationHash = context.ReadInt32(0xd0);
    WaterRenderableHash = context.ReadInt32(0xd4);
}

void Water13DefNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);
    context.WriteInt32(buffer, WaterSimulationHash, 0xd0);
    context.WriteInt32(buffer, WaterRenderableHash, 0xd4);
    context.WriteInt32(buffer, -0x4FF4E1AB, 0xe8);
    context.WriteInt32(buffer, -0x4FF4E1AB, 0xec);
}

int Water13DefNode::GetSizeForSerialization() const
{
    return 0xf0;
}

} // namespace SlLib::Resources::Scene
