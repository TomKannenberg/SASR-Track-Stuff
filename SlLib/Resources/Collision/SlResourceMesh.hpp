#pragma once

#include <memory>
#include <vector>

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Collision {
class SlCollisionMaterial;
class SlResourceMeshSection;

class SlResourceMesh final : public Serialization::IResourceSerializable
{
public:
    std::vector<std::shared_ptr<SlCollisionMaterial>> Materials;
    std::vector<std::shared_ptr<SlResourceMeshSection>> Sections;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Collision
