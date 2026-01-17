#pragma once

#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Scene::Definitions {

class SeAudio_Wwise_Environment_DefinitionNode final : public SeDefinitionTransformNode
{
public:
    std::string EnvName;
    int VolumeRTCP = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
