#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"

#include "SlLib/Math/Vector.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Scene {

class SeDefinitionTransformNode : public SeDefinitionNode
{
public:
    Math::Vector3 Translation;
    Math::Quaternion Rotation;
    Math::Vector3 Scale{1.0f, 1.0f, 1.0f};
    int InheritTransforms = 1;
    int TransformFlags = 0x7fe;

protected:
    int LoadInternal(Serialization::ResourceLoadContext& context, int offset) override;

public:
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
