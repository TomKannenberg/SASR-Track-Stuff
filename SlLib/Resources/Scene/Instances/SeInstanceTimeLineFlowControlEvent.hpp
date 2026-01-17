#pragma once

#include "SlLib/Resources/Scene/Instances/SeInstanceTimeLineEventNodeBase.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceTimeLineFlowControlEvent final : public SeInstanceTimeLineEventNodeBase
{
public:
    SeInstanceTimeLineFlowControlEvent();
    ~SeInstanceTimeLineFlowControlEvent() override;
};

} // namespace SlLib::Resources::Scene
