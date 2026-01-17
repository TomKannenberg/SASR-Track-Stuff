#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class DynamicObjectInstanceNode final : public SeInstanceEntityNode
{
public:
    DynamicObjectInstanceNode();
    ~DynamicObjectInstanceNode() override;
};

} // namespace SlLib::Resources::Scene
