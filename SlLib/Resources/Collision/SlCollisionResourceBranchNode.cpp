#include "SlCollisionResourceBranchNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>

namespace SlLib::Resources::Collision {

void SlCollisionResourceBranchNode::Load(Serialization::ResourceLoadContext& context)
{
    const std::size_t base = context.Position;
    Center = context.ReadFloat4(base + 0x0);
    Extents = context.ReadFloat4(base + 0x10);
    First = context.ReadInt32(base + 0x20);
    Next = context.ReadInt32(base + 0x24);
    Leaf = context.ReadInt32(base + 0x28);
    Flags = context.ReadInt32(base + 0x2c);
}

void SlCollisionResourceBranchNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteFloat4(buffer, Center, base + 0x0);
    context.WriteFloat4(buffer, Extents, base + 0x10);
    context.WriteInt32(buffer, First, base + 0x20);
    context.WriteInt32(buffer, Next, base + 0x24);
    context.WriteInt32(buffer, Leaf, base + 0x28);
    context.WriteInt32(buffer, Flags, base + 0x2c);
}

int SlCollisionResourceBranchNode::GetSizeForSerialization() const
{
    return 0x30;
}

} // namespace SlLib::Resources::Collision
