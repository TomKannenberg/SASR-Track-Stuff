#include "SeDefinitionParticleAffectorTurbulanceNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionParticleAffectorTurbulanceNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionParticleAffectorNode::Load(context);

    Magnitude = context.ReadFloat4(0x110);
    Frequency = context.ReadFloat4(0x120);
    Speed = context.ReadFloat4(0x130);
    Offset = context.ReadFloat4(0x140);
}

void SeDefinitionParticleAffectorTurbulanceNode::Save(Serialization::ResourceSaveContext& context,
    Serialization::ISaveBuffer& buffer)
{
    SeDefinitionParticleAffectorNode::Save(context, buffer);

    context.WriteFloat4(buffer, Magnitude, 0x110);
    context.WriteFloat4(buffer, Frequency, 0x120);
    context.WriteFloat4(buffer, Speed, 0x130);
    context.WriteFloat4(buffer, Offset, 0x140);
}

int SeDefinitionParticleAffectorTurbulanceNode::GetSizeForSerialization() const
{
    return 0x150;
}

} // namespace SlLib::Resources::Scene::Definitions
