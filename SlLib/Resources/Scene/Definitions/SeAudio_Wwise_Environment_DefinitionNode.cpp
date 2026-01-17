#include "SeAudio_Wwise_Environment_DefinitionNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeAudio_Wwise_Environment_DefinitionNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);

    EnvName = context.ReadStringPointer(0xd4);
    VolumeRTCP = context.ReadInt32(0xf4);
}

void SeAudio_Wwise_Environment_DefinitionNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);

    context.WriteStringPointer(buffer, EnvName, 0xd4);
    context.WriteInt32(buffer, VolumeRTCP, 0xf4);
}

int SeAudio_Wwise_Environment_DefinitionNode::GetSizeForSerialization() const
{
    return 0x100;
}

} // namespace SlLib::Resources::Scene::Definitions
