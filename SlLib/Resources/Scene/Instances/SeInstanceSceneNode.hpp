#pragma once

#include "SlLib/Resources/Scene/Instances/SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceSceneNode : public SeInstanceTransformNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
