#include "SeInstanceTransformNode.hpp"

#include "SlLib/Serialization/ISaveBuffer.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

SeInstanceTransformNode::SeInstanceTransformNode() = default;
SeInstanceTransformNode::~SeInstanceTransformNode() = default;

int SeInstanceTransformNode::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    return SeInstanceNode::LoadInternal(context, offset);
}

void SeInstanceTransformNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeInstanceNode::Save(context, buffer);
}

int SeInstanceTransformNode::GetSizeForSerialization() const
{
    return SeInstanceNode::GetSizeForSerialization();
}

} // namespace SlLib::Resources::Scene
