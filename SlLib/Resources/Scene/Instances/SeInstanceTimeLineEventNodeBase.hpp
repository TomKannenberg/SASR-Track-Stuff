#pragma once

#include "SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceTimeLineEventNodeBase : public SeInstanceTransformNode
{
public:
    SeInstanceTimeLineEventNodeBase();
    ~SeInstanceTimeLineEventNodeBase() override;
};

} // namespace SlLib::Resources::Scene
