#pragma once

#include "SlLib/Resources/Scene/Definitions/SeDefinitionTimeLineEventBaseNode.hpp"

namespace SlLib::Resources::Scene {

class SeDefinitionTimeLineFlowControlEvent : public SeDefinitionTimeLineEventBaseNode
{
public:
    bool AutoLoopCountReset = false;
    int LoopCount = 0;

protected:
    int LoadInternal(Serialization::ResourceLoadContext& context, int offset) override;

public:
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
