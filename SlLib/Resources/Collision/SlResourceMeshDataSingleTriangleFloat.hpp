#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Collision {

class SlResourceMeshDataSingleTriangleFloat final : public Serialization::IResourceSerializable
{
public:
    Math::Vector3 Center{};
    Math::Vector3 Max{};
    Math::Vector3 Min{};
    Math::Vector3 A{};
    Math::Vector3 B{};
    Math::Vector3 C{};
    int CollisionMaterialIndex = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Collision
