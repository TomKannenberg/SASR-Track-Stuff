#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeAudio_Wwise_Event_Volume_InstanceNode final : public SeInstanceEntityNode
{
public:
    SeAudio_Wwise_Event_Volume_InstanceNode();
    ~SeAudio_Wwise_Event_Volume_InstanceNode() override;
};

} // namespace SlLib::Resources::Scene
