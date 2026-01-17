#include "DynamicObjectDefinitionNode.hpp"

namespace SlLib::Resources::Scene::Definitions {

int DynamicObjectDefinitionNode::GetSizeForSerialization() const
{
    return 0xd0;
}

} // namespace SlLib::Resources::Scene::Definitions
