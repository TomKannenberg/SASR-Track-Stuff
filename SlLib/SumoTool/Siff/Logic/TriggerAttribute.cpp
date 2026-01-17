#include "TriggerAttribute.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::Logic {

TriggerAttribute::TriggerAttribute() = default;
TriggerAttribute::~TriggerAttribute() = default;

void TriggerAttribute::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    NameHash = context.ReadInt32();
    PackedValue = context.ReadInt32();
}

void TriggerAttribute::Save(SlLib::Serialization::ResourceSaveContext&,
                            SlLib::Serialization::ISaveBuffer&)
{
}

int TriggerAttribute::GetSizeForSerialization() const
{
    return 0x8;
}

} // namespace SlLib::SumoTool::Siff::Logic
