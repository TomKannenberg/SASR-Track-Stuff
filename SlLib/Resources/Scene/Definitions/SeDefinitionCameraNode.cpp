#include "SeDefinitionCameraNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene::Definitions {

void SeDefinitionCameraNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);

    VerticalFov = context.ReadFloat(0xd0);
    Aspect = context.ReadFloat(0xd4);
    NearPlane = context.ReadFloat(0xd8);
    FarPlane = context.ReadFloat(0xdc);
    OrthographicScale = context.ReadFloat2(0xe0);
    CameraFlags = context.ReadInt32(0xe8);
}

void SeDefinitionCameraNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);

    context.WriteFloat(buffer, VerticalFov, 0xd0);
    context.WriteFloat(buffer, Aspect, 0xd4);
    context.WriteFloat(buffer, NearPlane, 0xd8);
    context.WriteFloat(buffer, FarPlane, 0xdc);
    context.WriteFloat2(buffer, OrthographicScale, 0xe0);
    context.WriteInt32(buffer, CameraFlags, 0xe8);
    context.WriteInt32(buffer, 0xBADF00D, 0xec);
}

int SeDefinitionCameraNode::GetSizeForSerialization() const
{
    return 0xf0;
}

} // namespace SlLib::Resources::Scene::Definitions
