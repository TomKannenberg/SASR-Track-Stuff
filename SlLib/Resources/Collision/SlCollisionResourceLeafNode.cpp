#include "SlCollisionResourceLeafNode.hpp"

#include "SlLib/Resources/Collision/SlResourceMeshDataSingleTriangleFloat.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>
#include <stdexcept>

namespace SlLib::Resources::Collision {

void SlCollisionResourceLeafNode::Load(Serialization::ResourceLoadContext& context)
{
    context.ReadInt32();
    auto pointer = context.LoadPointer<SlResourceMeshDataSingleTriangleFloat>();
    if (!pointer)
        throw std::runtime_error("Triangle data cannot be NULL!");

    Data = std::move(pointer);
    context.ReadInt32();
}

void SlCollisionResourceLeafNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteInt32(buffer, 1, base + 0x0);
    context.SavePointer(buffer, Data.get(), base + 0x4);
}

int SlCollisionResourceLeafNode::GetSizeForSerialization() const
{
    return 0xc;
}

} // namespace SlLib::Resources::Collision
