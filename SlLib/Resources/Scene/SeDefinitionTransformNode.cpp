#include "SeDefinitionTransformNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

namespace SlLib::Resources::Scene {

int SeDefinitionTransformNode::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    offset = SeDefinitionNode::LoadInternal(context, offset);

    auto matrix = context.ReadMatrix(0x80);
    Translation = {matrix(0, 3), matrix(1, 3), matrix(2, 3)};

    InheritTransforms = context.ReadBitset32(0xc0);
    TransformFlags = context.ReadBitset32(0xc4);

    return offset + 0x50;
}

void SeDefinitionTransformNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeDefinitionNode::Save(context, buffer);

    Math::Matrix4x4 local{};
    local(0, 0) = Scale.X;
    local(1, 1) = Scale.Y;
    local(2, 2) = Scale.Z;
    local(0, 3) = Translation.X;
    local(1, 3) = Translation.Y;
    local(2, 3) = Translation.Z;

    context.WriteMatrix(buffer, local, 0x80);
    context.WriteInt32(buffer, InheritTransforms, 0xc0);
    context.WriteInt32(buffer, TransformFlags, 0xc4);
}

int SeDefinitionTransformNode::GetSizeForSerialization() const
{
    return 0xd0;
}

} // namespace SlLib::Resources::Scene
