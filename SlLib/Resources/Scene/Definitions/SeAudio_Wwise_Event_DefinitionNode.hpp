#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Utilities {
int HashString(std::string_view input) noexcept;
}

namespace SlLib::Resources::Scene::Definitions {

class SeAudio_Wwise_Event_DefinitionNode : public SeDefinitionTransformNode
{
public:
    std::string EventName;
    bool Static = false;
    int TriggerType = 0;
    bool SimultaneousPlay = false;
    int GameEvent = 0;
    float AttenuationScale = 1.0f;
    bool Environmental = true;
    bool AreaSound = false;
    int EventPicker = 0;
    int ComboIndex = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;

private:
    int _prevComboIndex = 0;
};

} // namespace SlLib::Resources::Scene::Definitions
