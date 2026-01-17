#pragma once

#include "SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceTimelineNode final : public SeInstanceTransformNode
{
public:
    SeInstanceTimelineNode();
    ~SeInstanceTimelineNode() override;
};

} // namespace SlLib::Resources::Scene
