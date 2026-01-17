#pragma once

#include "SlLib/Resources/Scene/SeInstanceNode.hpp"

namespace SlLib::Resources::Scene::Dummies {

class SeDummyInstanceNode final : public SeInstanceNode
{
public:
    SeDummyInstanceNode();
    ~SeDummyInstanceNode() override;
};

} // namespace SlLib::Resources::Scene::Dummies
