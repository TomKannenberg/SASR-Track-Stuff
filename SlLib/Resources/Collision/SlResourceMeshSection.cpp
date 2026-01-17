#include "SlResourceMeshSection.hpp"

#include "SlLib/Resources/Collision/SlCollisionResourceBranchNode.hpp"
#include "SlLib/Resources/Collision/SlCollisionResourceLeafNode.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>
#include <stdexcept>

namespace SlLib::Resources::Collision {

void SlResourceMeshSection::Load(Serialization::ResourceLoadContext& context)
{
    Type = context.ReadInt32();
    if (Type != 2)
        throw std::runtime_error("Only single triangle collision mesh data is supported!");

    int numBranches = context.ReadInt32();
    Roots = context.ReadInt32();
    Branches = context.LoadPointerArray<SlCollisionResourceBranchNode>(numBranches);
    Leafs = context.LoadPointerArray<SlCollisionResourceLeafNode>(context.ReadInt32());
}

void SlResourceMeshSection::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    auto sneakyData = context.Allocate(8, 1);
    context.WriteInt32(sneakyData, static_cast<int>(Branches.size()), 0x0);
    context.WriteInt32(sneakyData, Roots, 0x4);

    context.WriteInt32(buffer, Type, 0x0);
    context.WriteInt32(buffer, static_cast<int>(Branches.size()), 0x4);
    context.WriteInt32(buffer, Roots, 0x8);
    context.WriteInt32(buffer, static_cast<int>(Leafs.size()), 0x10);

    context.SaveReferenceArray(buffer, Branches, 0xc, 0x10);
    context.SaveReferenceArray(buffer, Leafs, 0x14);
}

int SlResourceMeshSection::GetSizeForSerialization() const
{
    return 0x18;
}

} // namespace SlLib::Resources::Collision
