#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"

namespace SlLib::Resources::Scene::Dummies {

class SeDummyDefinitionNode final : public SeDefinitionNode
{
public:
    SeDummyDefinitionNode();
    ~SeDummyDefinitionNode() override;
};

} // namespace SlLib::Resources::Scene::Dummies
