#include "SlResourceHeader.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"
#include "SlLib/Utilities/SlUtil.hpp"

#include <cstddef>

namespace SlLib::Resources::Database {

namespace {
constexpr int AndroidVersionThreshold = 0x27;
}

void SlResourceHeader::Load(Serialization::ResourceLoadContext& context)
{
    Platform = context.Platform;
    const std::size_t base = context.Position;

    Id = context.ReadInt32(base + 0x0);
    if (context.Version >= AndroidVersionThreshold)
    {
        Ref = context.ReadInt32(base + 0x4);
        Name = context.ReadStringPointer(base + 0x8);
    }
    else
    {
        Name = context.ReadStringPointer(base + 0x4);
        Ref = context.ReadInt32(base + 0x8);
    }
}

void SlResourceHeader::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    const std::size_t base = buffer.Position;
    context.WriteInt32(buffer, Id, base + 0x0);
    context.WriteStringPointer(buffer, Name, base + 0x4);
    context.WriteInt32(buffer, Ref, base + 0x8);
}

int SlResourceHeader::GetSizeForSerialization() const
{
    if (Platform && Platform->Is64Bit())
        return 0x10;

    return 0xc;
}

void SlResourceHeader::SetName(std::string_view tag)
{
    Name.assign(tag);
    Id = Utilities::HashString(tag);
}

} // namespace SlLib::Resources::Database
