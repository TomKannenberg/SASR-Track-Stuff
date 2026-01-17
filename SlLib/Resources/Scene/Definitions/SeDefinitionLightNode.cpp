#include "SeDefinitionLightNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionLightNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);
}

int SeDefinitionLightNode::GetSizeForSerialization() const
{
    return 0x100;
}

} // namespace SlLib::Resources::Scene::Definitions
