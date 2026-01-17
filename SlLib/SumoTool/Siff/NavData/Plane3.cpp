#include "Plane3.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::NavData {

namespace {

SlLib::Math::Vector3 ReadAlignedFloat3(SlLib::Serialization::ResourceLoadContext& context)
{
    auto value = context.ReadFloat3();
    context.Position += 4;
    return value;
}

} // namespace

Plane3::Plane3() = default;
Plane3::~Plane3() = default;

void Plane3::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Normal = ReadAlignedFloat3(context);
    Const = context.ReadFloat();
    context.Position += 0x0c;
}

void Plane3::Save(SlLib::Serialization::ResourceSaveContext&,
                  SlLib::Serialization::ISaveBuffer&)
{
}

int Plane3::GetSizeForSerialization() const
{
    return 0x20;
}

} // namespace SlLib::SumoTool::Siff::NavData
