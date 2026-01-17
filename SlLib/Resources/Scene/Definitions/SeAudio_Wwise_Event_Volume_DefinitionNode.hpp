#pragma once

#include "SeAudio_Wwise_Event_DefinitionNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeAudio_Wwise_Event_Volume_DefinitionNode final : public SeAudio_Wwise_Event_DefinitionNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
