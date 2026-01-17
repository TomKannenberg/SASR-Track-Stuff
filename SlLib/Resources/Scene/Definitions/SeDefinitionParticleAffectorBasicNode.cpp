#include "SeDefinitionParticleAffectorBasicNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionParticleAffectorBasicNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionParticleAffectorNode::Load(context);
    ForceMode = context.ReadInt32(0x110);
}

void SeDefinitionParticleAffectorBasicNode::Save(Serialization::ResourceSaveContext& context,
    Serialization::ISaveBuffer& buffer)
{
    SeDefinitionParticleAffectorNode::Save(context, buffer);
    context.WriteInt32(buffer, ForceMode, 0x110);
}

int SeDefinitionParticleAffectorBasicNode::GetSizeForSerialization() const
{
    return 0x120;
}

} // namespace SlLib::Resources::Scene::Definitions
