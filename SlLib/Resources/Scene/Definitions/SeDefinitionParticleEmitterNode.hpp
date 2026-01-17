#pragma once

#include <string>

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"
#include "SlLib/Resources/Scene/SeDefinitionTransformNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionParticleEmitterNode : public SeDefinitionTransformNode
{
public:
    int SlParticleEmitterTimingParamsFlags = 0;
    float SpawnRate = 0.0f;
    float SpawnDuration = 0.0f;
    float SpawnDurationRandom = 0.0f;
    int NumSpawns = 0;
    float SpawnDelay = 0.0f;
    float SpawnDelayRandom = 0.0f;
    float InitialDelay = 0.0f;
    float InitialDelayRandom = 0.0f;
    float PPM = 0.0f;
    float SpawnRateNoiseAdd = 0.0f;
    float SpawnRateNoiseSpeed = 0.0f;
    Math::Vector4 PositionAngle{};
    Math::Vector4 PositionAngleRandom{};
    Math::Vector4 VelocitySpin{};
    Math::Vector4 VelocitySpinRandom{};
    int EmitterShape = 0;
    int SpawnParamsFlags = 0;
    float RadiusA = 0.0f;
    float RadiusB = 0.0f;
    float RadialStart = 0.0f;
    float RadialSize = 0.0f;
    float PolarStart = 0.0f;
    float PolarSize = 0.0f;
    float EmitterVelocityMul = 0.0f;
    float OffsetU = 0.0f;
    float OffsetV = 0.0f;
    SlLib::Resources::Scene::SeDefinitionNode* ParticleStyle = nullptr;
    std::string ParticleStyleName;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
