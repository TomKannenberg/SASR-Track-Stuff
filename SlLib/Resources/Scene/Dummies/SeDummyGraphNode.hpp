#pragma once

#include "../SeGraphNode.hpp"

namespace SlLib::Resources::Scene::Dummies {

class SeDummyGraphNode final : public SeGraphNode
{
public:
    SeDummyGraphNode();
    ~SeDummyGraphNode() override;
};

} // namespace SlLib::Resources::Scene::Dummies
