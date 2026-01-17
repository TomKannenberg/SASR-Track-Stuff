#pragma once

#include "SlLib/Resources/Scene/Definitions/SeDefinitionSceneNode.hpp"

namespace SlLib::Resources::Scene {

class Water13SurfaceWavesDefNode final : public SeDefinitionSceneNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
