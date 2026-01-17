#include "SeInstanceNode.hpp"
#include "SeDefinitionNode.hpp"

#include <algorithm>

namespace SlLib::Resources::Scene {

SeDefinitionNode::SeDefinitionNode() = default;

void SeDefinitionNode::RegisterInstance(SeInstanceNode* instance)
{
    if (instance != nullptr) {
        Instances.push_back(instance);
    }
}

void SeDefinitionNode::UnregisterInstance(SeInstanceNode* instance)
{
    Instances.erase(std::remove(Instances.begin(), Instances.end(), instance), Instances.end());
}

SeInstanceNode* SeDefinitionNode::Instance(SeGraphNode* parent)
{
    SeInstanceNode* instance = SeInstanceNode::CreateObject<SeInstanceNode>(this);
    if (parent != nullptr) {
        instance->SetParent(parent);
    }

    SeGraphNode* child = FirstChild;
    while (child != nullptr) {
        if (auto definition = dynamic_cast<SeDefinitionNode*>(child)) {
            definition->Instance(instance);
        }
        child = child->NextSibling;
    }

    return instance;
}

} // namespace SlLib::Resources::Scene
