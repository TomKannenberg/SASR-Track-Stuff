#pragma once

#include "SeGraphNode.hpp"

#include <memory>
#include <vector>

namespace SlLib::Resources::Scene {

class SeInstanceNode;

class SeDefinitionNode : public SeGraphNode
{
public:
    SeDefinitionNode();

    std::vector<SeInstanceNode*> Instances;

    SeInstanceNode* Instance(SeGraphNode* parent);

    void RegisterInstance(SeInstanceNode* instance);
    void UnregisterInstance(SeInstanceNode* instance);
};

} // namespace SlLib::Resources::Scene
