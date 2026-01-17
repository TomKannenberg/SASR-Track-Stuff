#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeAudio_Wwise_ParamControl_InstanceNode final : public SeInstanceEntityNode
{
public:
    SeAudio_Wwise_ParamControl_InstanceNode();
    ~SeAudio_Wwise_ParamControl_InstanceNode() override;
};

} // namespace SlLib::Resources::Scene
