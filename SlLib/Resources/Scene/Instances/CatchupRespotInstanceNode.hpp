#pragma once

#include "SlLib/Resources/Scene/Instances/SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class CatchupRespotInstanceNode final : public SeInstanceTransformNode
{
public:
    CatchupRespotInstanceNode();
    ~CatchupRespotInstanceNode() override;
};

} // namespace SlLib::Resources::Scene
