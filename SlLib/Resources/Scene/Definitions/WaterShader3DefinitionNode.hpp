#pragma once

#include "ShaderDefinitionBaseNode.hpp"

namespace SlLib::Resources::Scene {

class WaterShader3DefinitionNode final : public ShaderDefinitionBaseNode
{
public:
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::Resources::Scene
