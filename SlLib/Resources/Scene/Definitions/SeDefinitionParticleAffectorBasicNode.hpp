#pragma once

#include "SlLib/Resources/Scene/Definitions/SeDefinitionParticleAffectorNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionParticleAffectorBasicNode final : public SeDefinitionParticleAffectorNode
{
public:
    int ForceMode = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
