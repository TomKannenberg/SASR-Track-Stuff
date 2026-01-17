#pragma once

#include "SeInstanceSceneNode.hpp"

namespace SlLib::Resources::Scene {

class TriggerPhantomInstanceNode final : public SeInstanceSceneNode
{
public:
    TriggerPhantomInstanceNode();
    ~TriggerPhantomInstanceNode() override;
};

} // namespace SlLib::Resources::Scene
