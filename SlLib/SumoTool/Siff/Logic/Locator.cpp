#include "Locator.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::SumoTool::Siff::Logic {

Locator::Locator() = default;
Locator::~Locator() = default;

void Locator::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    GroupNameHash = context.ReadInt32();
    LocatorNameHash = context.ReadInt32();
    MeshForestNameHash = context.ReadInt32();
    MeshTreeNameHash = context.ReadInt32();
    SetupObjectNameHash = context.ReadInt32();
    AnimatedInstanceNameHash = context.ReadInt32();
    SubDataHash = context.ReadInt32();
    Flags = context.ReadInt32();
    Health = context.ReadInt32();
    SequenceStartFrameMultiplier = context.ReadFloat();
    SequencerInterSpawnMultiplier = context.ReadFloat();
    AnimatedInstancePlaybackSpeed = context.ReadFloat();
    PositionAsFloats = context.ReadFloat4();
    RotationAsFloats = context.ReadFloat4();
}

void Locator::Save(SlLib::Serialization::ResourceSaveContext&,
                   SlLib::Serialization::ISaveBuffer&)
{
}

int Locator::GetSizeForSerialization() const
{
    return 0x50;
}

} // namespace SlLib::SumoTool::Siff::Logic
