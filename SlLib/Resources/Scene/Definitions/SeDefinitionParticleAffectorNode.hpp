#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionParticleAffectorNode : public SeDefinitionTransformNode
{
public:
    float Force = 0.0f;
    float ForceRand = 0.0f;
    float RandomSpeed = 0.0f;
    int FalloffMode = 0;
    float FalloffPower = 0.0f;
    float RadiusA = 0.0f;
    int AffectorSetID = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
