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
class SlCollisionResourceBranchNode;
class SlCollisionResourceLeafNode;

class SlResourceMeshSection final : public Serialization::IResourceSerializable
{
public:
    int Type = 2;
    int Roots = 0;
    std::vector<std::shared_ptr<SlCollisionResourceBranchNode>> Branches;
    std::vector<std::shared_ptr<SlCollisionResourceLeafNode>> Leafs;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Collision
