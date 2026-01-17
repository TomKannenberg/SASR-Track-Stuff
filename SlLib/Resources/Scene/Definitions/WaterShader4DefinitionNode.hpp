#pragma once

#include "ShaderDefinitionBaseNode.hpp"

namespace SlLib::Resources::Scene {

class WaterShader4DefinitionNode final : public ShaderDefinitionBaseNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
