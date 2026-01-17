#pragma once

#include "SlLib/Resources/Scene/Instances/SeInstanceTransformNode.hpp"

namespace SlLib::Resources::Scene {

class CameoObjectInstanceNode final : public SeInstanceTransformNode
{
public:
    CameoObjectInstanceNode();
    ~CameoObjectInstanceNode() override;
};

} // namespace SlLib::Resources::Scene
