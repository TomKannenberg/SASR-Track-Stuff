#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Collision {

class SlCollisionResourceBranchNode final : public Serialization::IResourceSerializable
{
public:
    Math::Vector4 Center{};
    Math::Vector4 Extents{};
    int First = -1;
    int Next = -1;
    int Leaf = -1;
    int Flags = 1;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Collision
