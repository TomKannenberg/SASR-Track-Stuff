#pragma once

#include <memory>

#include "SlLib/Resources/Collision/SlResourceMeshDataSingleTriangleFloat.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Collision {
class SlCollisionResourceLeafNode final : public Serialization::IResourceSerializable
{
public:
    std::unique_ptr<SlResourceMeshDataSingleTriangleFloat> Data;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Collision
