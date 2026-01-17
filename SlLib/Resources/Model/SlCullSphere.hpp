#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Resources::Model {

class SlCullSphere : public Serialization::IResourceSerializable
{
public:
    Math::Vector3 SphereCenter{};
    float Radius = 1.0f;
    Math::Vector4 BoxCenter{0.0f, 0.0f, 0.0f, 1.0f};
    Math::Vector4 Extents{0.5f, 0.5f, 0.5f, 0.0f};

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Model
