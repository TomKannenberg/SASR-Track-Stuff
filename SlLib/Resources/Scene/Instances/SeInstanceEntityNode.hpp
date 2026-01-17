#pragma once

#include "SlLib/Resources/Scene/SeInstanceNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceEntityNode : public SeInstanceNode
{
public:
    SeInstanceEntityNode();
    ~SeInstanceEntityNode() override;

    int RenderLayer = 0;
    int TransformFlags = 0;
};

} // namespace SlLib::Resources::Scene
