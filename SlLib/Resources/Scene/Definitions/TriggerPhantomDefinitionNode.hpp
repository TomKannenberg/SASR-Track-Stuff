#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Scene {

class TriggerPhantomDefinitionNode : public SeDefinitionTransformNode
{
public:
    float WidthRadius = 0.0f;
    float Height = 0.0f;
    float Depth = 0.0f;
    int Shape = 0;
    int SendChildMessages = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
