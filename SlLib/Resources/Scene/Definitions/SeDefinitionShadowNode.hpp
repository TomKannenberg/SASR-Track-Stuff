#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionShadowNode final : public SeDefinitionTransformNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
