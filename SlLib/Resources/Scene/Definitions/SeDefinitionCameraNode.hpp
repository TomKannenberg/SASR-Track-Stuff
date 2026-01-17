#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionCameraNode final : public SeDefinitionTransformNode
{
public:
    float VerticalFov = 60.0f;
    float Aspect = 16.0f / 9.0f;
    float NearPlane = 0.1f;
    float FarPlane = 20000.0f;
    Math::Vector2 OrthographicScale{1.0f, 1.0f};
    int CameraFlags = 1;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
