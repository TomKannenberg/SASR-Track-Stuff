#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceParticleEmitterNode final : public SeInstanceEntityNode
{
public:
    SeInstanceParticleEmitterNode();
    ~SeInstanceParticleEmitterNode() override;
};

} // namespace SlLib::Resources::Scene
