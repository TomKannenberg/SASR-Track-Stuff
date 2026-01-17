#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeFSAAInstanceNode final : public SeInstanceEntityNode
{
public:
    SeFSAAInstanceNode();
    ~SeFSAAInstanceNode() override;
};

} // namespace SlLib::Resources::Scene
