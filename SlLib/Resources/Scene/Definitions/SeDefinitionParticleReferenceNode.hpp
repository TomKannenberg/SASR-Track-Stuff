#pragma once

#include <string>

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"
#include "../Instances/SeInstanceParticleSystemNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionParticleReferenceNode final : public SeDefinitionTransformNode
{
public:
    std::string ReferenceSystemName;
    SlLib::Resources::Scene::SeInstanceParticleSystemNode* ReferenceSystem = nullptr;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
