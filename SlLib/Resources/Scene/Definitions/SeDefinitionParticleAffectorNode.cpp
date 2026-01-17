#include "SeDefinitionParticleAffectorNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionParticleAffectorNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);

    Force = context.ReadFloat(0xd0);
    ForceRand = context.ReadFloat(0xd4);
    RandomSpeed = context.ReadFloat(0xd8);
    FalloffMode = context.ReadInt32(0xdc);
    FalloffPower = context.ReadFloat(0xe0);
    RadiusA = context.ReadFloat(0xe4);
    AffectorSetID = context.ReadInt32(0x100);
}

void SeDefinitionParticleAffectorNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);

    context.WriteFloat(buffer, Force, 0xd0);
    context.WriteFloat(buffer, ForceRand, 0xd4);
    context.WriteFloat(buffer, RandomSpeed, 0xd8);
    context.WriteInt32(buffer, FalloffMode, 0xdc);
    context.WriteFloat(buffer, FalloffPower, 0xe0);
    context.WriteFloat(buffer, RadiusA, 0xe4);
    context.WriteFloat(buffer, 1.0f, 0xe8);
    context.WriteInt32(buffer, AffectorSetID, 0x100);
}

int SeDefinitionParticleAffectorNode::GetSizeForSerialization() const
{
    return 0x110;
}

} // namespace SlLib::Resources::Scene::Definitions
