#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"

namespace SlLib::Resources::Scene {

class SeFogDefinitionNode : public SeDefinitionNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
