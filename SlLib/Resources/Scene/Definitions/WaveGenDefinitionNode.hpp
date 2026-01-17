#pragma once

#include "SlLib/Resources/Scene/Definitions/SeDefinitionSceneNode.hpp"

namespace SlLib::Resources::Scene {

class WaveGenDefinitionNode final : public SeDefinitionSceneNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
