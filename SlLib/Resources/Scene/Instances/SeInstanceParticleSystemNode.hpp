#pragma once

#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene {

class SeInstanceParticleSystemNode final : public SeInstanceEntityNode
{
public:
    SeInstanceParticleSystemNode();
    ~SeInstanceParticleSystemNode() override;
};

} // namespace SlLib::Resources::Scene
