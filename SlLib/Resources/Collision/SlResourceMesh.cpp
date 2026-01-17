#include "SlResourceMesh.hpp"

#include "SlLib/Resources/Collision/SlCollisionMaterial.hpp"
#include "SlLib/Resources/Collision/SlResourceMeshSection.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>

namespace SlLib::Resources::Collision {

void SlResourceMesh::Load(Serialization::ResourceLoadContext& context)
{
    Materials = context.LoadPointerArray<SlCollisionMaterial>(context.ReadInt32());
    Sections = context.LoadPointerArray<SlResourceMeshSection>(context.ReadInt32());
}

void SlResourceMesh::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t materialCount = Materials.size();
    const std::size_t sectionCount = Sections.size();

    context.WriteInt32(buffer, static_cast<int>(materialCount), 0x0);
    context.WriteInt32(buffer, static_cast<int>(sectionCount), 0x8);

    auto materialData = context.SaveGenericPointer(buffer, 0x4, (0x4 + 0x10) * materialCount, 1);
    for (std::size_t i = 0; i < materialCount; ++i)
    {
        std::size_t pointerOffset = i * 4;
        std::size_t dataOffset = (0x4 * materialCount) + (i * 0x10);
        context.SaveReference(materialData, Materials[i].get(), static_cast<int>(dataOffset));
        context.SavePointer(materialData, Materials[i].get(), static_cast<int>(pointerOffset));
    }

    auto sectionData = context.SaveGenericPointer(buffer, 0xc, (0x4 + 0x18) * sectionCount, 1);
    for (std::size_t i = 0; i < sectionCount; ++i)
    {
        std::size_t pointerOffset = i * 4;
        std::size_t dataOffset = (0x4 * sectionCount) + (i * 0x18);
        context.SaveReference(sectionData, Sections[i].get(), static_cast<int>(dataOffset));
        context.SavePointer(sectionData, Sections[i].get(), static_cast<int>(pointerOffset));
    }
}

int SlResourceMesh::GetSizeForSerialization() const
{
    return 0x10;
}

} // namespace SlLib::Resources::Collision
