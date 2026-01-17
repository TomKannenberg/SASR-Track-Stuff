#pragma once

#include "SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceLocatorNode final : public SeInstanceTransformNode
{
public:
    SeInstanceLocatorNode();
    ~SeInstanceLocatorNode() override;
};

} // namespace SlLib::Resources::Scene
