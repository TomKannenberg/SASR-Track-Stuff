#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Scene/Definitions/SeDefinitionParticleAffectorNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

class SeDefinitionParticleAffectorTurbulanceNode final : public SeDefinitionParticleAffectorNode
{
public:
    Math::Vector4 Magnitude{};
    Math::Vector4 Frequency{};
    Math::Vector4 Speed{};
    Math::Vector4 Offset{};

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene::Definitions
