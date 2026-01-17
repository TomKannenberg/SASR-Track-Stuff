#pragma once

#include "SlLib/Resources/Scene/Definitions/SeDefinitionTimeLineEventBaseNode.hpp"
#include "SlLib/Resources/Scene/SeNodeBase.hpp"

namespace SlLib::Resources::Scene {

class SeDefinitionTimeLineEventNode : public SeDefinitionTimeLineEventBaseNode
{
public:
    int StartMessageDestination = 0;
    int StartMessage = 0;
    SeNodeBase* StartRecipient = nullptr;
    int EndMessageDestination = 0;
    int EndMessage = 0;
    SeNodeBase* EndRecipient = nullptr;

protected:
    int LoadInternal(Serialization::ResourceLoadContext& context, int offset) override;

public:
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
