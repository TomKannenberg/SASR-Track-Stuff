#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionLightNode final : public SeDefinitionTransformNode
{
public:
    void Load(Serialization::ResourceLoadContext& context) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
