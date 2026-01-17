#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"

namespace SlLib::Resources::Scene {

class SeDefinitionTimeLineEventBaseNode : public SeDefinitionNode
{
public:
    float Start = 0.0f;
    float Duration = 0.0f;

protected:
    int LoadInternal(Serialization::ResourceLoadContext& context, int offset) override;

public:
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
