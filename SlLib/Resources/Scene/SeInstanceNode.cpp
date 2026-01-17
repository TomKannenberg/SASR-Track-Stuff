#include "SeInstanceNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"
#include "SlLib/Serialization/ISaveBuffer.hpp"

namespace SlLib::Resources::Scene {

SeInstanceNode::SeInstanceNode() = default;

SeInstanceNode::~SeInstanceNode()
{
    SetDefinition(nullptr);
}

SeDefinitionNode* SeInstanceNode::Definition() const
{
    return _definition;
}

void SeInstanceNode::SetDefinition(SeDefinitionNode* definition)
{
    if (_definition == definition) {
        return;
    }

    if (_definition != nullptr) {
        _definition->UnregisterInstance(this);
    }

    _definition = definition;

    if (_definition != nullptr) {
        _definition->RegisterInstance(this);
    }
}

int SeInstanceNode::LoadInternal(Serialization::ResourceLoadContext& context, int offset)
{
    offset = SeGraphNode::LoadInternal(context, offset);

    SeDefinitionNode* definition = static_cast<SeDefinitionNode*>(context.LoadNode(context.ReadInt32(offset)));
    SetDefinition(definition);
    LocalTimeScale = context.ReadFloat(0x70);
    LocalTime = context.ReadFloat(0x74);
    SceneGraphFlags = context.ReadInt32(0x78);
    return offset + 0x18;
}

void SeInstanceNode::Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer)
{
    SeGraphNode::Save(context, buffer);
    context.WriteNodePointer(buffer, _definition, 0x68);
    context.WriteFloat(buffer, LocalTimeScale, 0x70);
    context.WriteFloat(buffer, LocalTime, 0x74);
    context.WriteInt32(buffer, SceneGraphFlags, 0x78);
}

int SeInstanceNode::GetSizeForSerialization() const
{
    return 0x80;
}

SlLib::Enums::InstanceTimeStep SeInstanceNode::TimeStep() const
{
    return static_cast<SlLib::Enums::InstanceTimeStep>(SceneGraphFlags & 0xf);
}

void SeInstanceNode::SetTimeStep(SlLib::Enums::InstanceTimeStep value)
{
    SceneGraphFlags &= ~0xf;
    SceneGraphFlags |= (static_cast<int>(value) & 0xf);
}

} // namespace SlLib::Resources::Scene
