#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::Logic {

namespace Serialization = SlLib::Serialization;

class Locator final : public Serialization::IResourceSerializable
{
public:
    Locator();
    ~Locator();

    int GroupNameHash = 0;
    int LocatorNameHash = 0;
    int MeshForestNameHash = 0;
    int MeshTreeNameHash = 0;
    int SetupObjectNameHash = 0;
    int AnimatedInstanceNameHash = 0;
    int SubDataHash = 0;
    int Flags = 0;
    int Health = 0;
    float SequenceStartFrameMultiplier = 0.0f;
    float SequencerInterSpawnMultiplier = 0.0f;
    float AnimatedInstancePlaybackSpeed = 0.0f;
    SlLib::Math::Vector4 PositionAsFloats{};
    SlLib::Math::Vector4 RotationAsFloats{};

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::Logic
