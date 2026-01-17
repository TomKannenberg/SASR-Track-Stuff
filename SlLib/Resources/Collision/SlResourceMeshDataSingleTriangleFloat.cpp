#include "SlResourceMeshDataSingleTriangleFloat.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <algorithm>
#include <cstddef>

namespace SlLib::Resources::Collision {

static Math::Vector3 MaxVec(Math::Vector3 const& a, Math::Vector3 const& b)
{
    return {
        std::max(a.X, b.X),
        std::max(a.Y, b.Y),
        std::max(a.Z, b.Z),
    };
}

static Math::Vector3 MinVec(Math::Vector3 const& a, Math::Vector3 const& b)
{
    return {
        std::min(a.X, b.X),
        std::min(a.Y, b.Y),
        std::min(a.Z, b.Z),
    };
}

void SlResourceMeshDataSingleTriangleFloat::Load(Serialization::ResourceLoadContext& context)
{
    A = context.ReadFloat3();
    B = context.ReadFloat3();
    C = context.ReadFloat3();

    Center = (A + B + C) / 3.0f;
    Max = MaxVec(A, MaxVec(B, C));
    Min = MinVec(A, MinVec(B, C));

    CollisionMaterialIndex = context.ReadInt32();
}

void SlResourceMeshDataSingleTriangleFloat::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteFloat3(buffer, A, base + 0x0);
    context.WriteFloat3(buffer, B, base + 0xc);
    context.WriteFloat3(buffer, C, base + 0x18);
    context.WriteInt32(buffer, CollisionMaterialIndex, base + 0x24);
}

int SlResourceMeshDataSingleTriangleFloat::GetSizeForSerialization() const
{
    return 0x28;
}

} // namespace SlLib::Resources::Collision
