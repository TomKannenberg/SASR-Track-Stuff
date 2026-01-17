#include "SlCullSphere.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <cstddef>

namespace SlLib::Resources::Model {

void SlCullSphere::Load(Serialization::ResourceLoadContext& context)
{
    const std::size_t base = context.Position;
    SphereCenter = context.ReadFloat3(base + 0x0);
    Radius = context.ReadFloat(base + 0xc);
    BoxCenter = context.ReadFloat4(base + 0x10);
    Extents = context.ReadFloat4(base + 0x20);
}

void SlCullSphere::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteFloat3(buffer, SphereCenter, base + 0x0);
    context.WriteFloat(buffer, Radius, base + 0xc);
    context.WriteFloat4(buffer, BoxCenter, base + 0x10);
    context.WriteFloat4(buffer, Extents, base + 0x20);
}

int SlCullSphere::GetSizeForSerialization() const
{
    return 0x30;
}

} // namespace SlLib::Resources::Model
