#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeAudio_Wwise_Environment_InstanceNode final : public SeInstanceEntityNode
{
public:
    SeAudio_Wwise_Environment_InstanceNode();
    ~SeAudio_Wwise_Environment_InstanceNode() override;
};

} // namespace SlLib::Resources::Scene
