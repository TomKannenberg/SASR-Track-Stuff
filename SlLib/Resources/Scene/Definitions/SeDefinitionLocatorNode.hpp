#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeDefinitionLocatorNode : public SeDefinitionTransformNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
