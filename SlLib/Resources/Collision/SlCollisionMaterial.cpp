#include "SlCollisionMaterial.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>

namespace SlLib::Resources::Collision {

void SlCollisionMaterial::Load(Serialization::ResourceLoadContext& context)
{
    const std::size_t base = context.Position;
    Flags = static_cast<CollisionFlags>(context.ReadInt32(base + 0x0));
    Type = static_cast<SurfaceType>(context.ReadInt32(base + 0x4));
    Color = context.ReadInt32(base + 0x8);
    Name = context.ReadStringPointer(base + 0xc);
}

void SlCollisionMaterial::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteInt32(buffer, static_cast<int>(Flags), base + 0x0);
    context.WriteInt32(buffer, static_cast<int>(Type), base + 0x4);
    context.WriteInt32(buffer, Color, base + 0x8);
    context.WriteStringPointer(buffer, Name, base + 0xc);
}

int SlCollisionMaterial::GetSizeForSerialization() const
{
    return 0x10;
}

} // namespace SlLib::Resources::Collision
