#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"

namespace SlLib::Resources::Scene {

class TrafficManagerDefinitionNode : public SeDefinitionNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
