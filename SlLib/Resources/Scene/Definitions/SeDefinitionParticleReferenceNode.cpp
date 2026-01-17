#include "SeDefinitionParticleReferenceNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionParticleReferenceNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);

    ReferenceSystemName = context.ReadStringPointer(0xd0);
    ReferenceSystem = static_cast<SlLib::Resources::Scene::SeInstanceParticleSystemNode*>(
        context.LoadNode(context.ReadInt32(0xf8)));
}

void SeDefinitionParticleReferenceNode::Save(Serialization::ResourceSaveContext& context,
    Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);

    context.WriteStringPointer(buffer, ReferenceSystemName, 0xd0);
    context.WriteInt32(buffer, ReferenceSystem ? ReferenceSystem->Uid : 0, 0xf8);
}

int SeDefinitionParticleReferenceNode::GetSizeForSerialization() const
{
    return 0x100;
}

} // namespace SlLib::Resources::Scene::Definitions
