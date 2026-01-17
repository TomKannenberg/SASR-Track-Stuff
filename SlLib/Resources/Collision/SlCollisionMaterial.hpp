#pragma once

#include "SlLib/Enums/CollisionFlags.hpp"
#include "SlLib/Enums/SurfaceType.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

#include <string>

namespace SlLib::Serialization {
struct ISaveBuffer;
class ResourceLoadContext;
class ResourceSaveContext;
}

namespace SlLib::Resources::Collision {
using SlLib::Enums::CollisionFlags;
using SlLib::Enums::SurfaceType;

class SlCollisionMaterial final : public Serialization::IResourceSerializable
{
public:
    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;

    CollisionFlags Flags{};
    SurfaceType Type{};
    int Color = 0;
    std::string Name;
};

} // namespace SlLib::Resources::Collision
