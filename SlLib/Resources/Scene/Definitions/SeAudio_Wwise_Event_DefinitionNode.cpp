#include "SeAudio_Wwise_Event_DefinitionNode.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"
#include "SlLib/Utilities/SlUtil.hpp"

#include <cstddef>

namespace SlLib::Resources::Scene::Definitions {

void SeAudio_Wwise_Event_DefinitionNode::Load(Serialization::ResourceLoadContext& context)
{
    SeDefinitionTransformNode::Load(context);

    EventName = context.ReadStringPointer(0xd4);
    Static = context.ReadBoolean(0xf4);
    TriggerType = context.ReadInt32(0xfc);
    SimultaneousPlay = context.ReadBoolean(0x100);
    GameEvent = context.ReadInt32(0x104);
    AttenuationScale = context.ReadFloat(0x110);
    Environmental = context.ReadBoolean(0x114);
    AreaSound = context.ReadBoolean(0x118);
    EventPicker = context.ReadInt32(0x11c);
    ComboIndex = context.ReadInt32(0x11c);
    _prevComboIndex = context.ReadInt32(0x120);
}

void SeAudio_Wwise_Event_DefinitionNode::Save(Serialization::ResourceSaveContext& context,
    Serialization::ISaveBuffer& buffer)
{
    SeDefinitionTransformNode::Save(context, buffer);

    context.WriteStringPointer(buffer, EventName, 0xd4);
    context.WriteBoolean(buffer, Static, 0xf4);
    context.WriteInt32(buffer, TriggerType, 0xfc);
    context.WriteBoolean(buffer, SimultaneousPlay, 0x100);
    context.WriteInt32(buffer, GameEvent, 0x104);
    context.WriteFloat(buffer, AttenuationScale, 0x110);
    context.WriteBoolean(buffer, Environmental, 0x114);
    context.WriteBoolean(buffer, AreaSound, 0x118);
    context.WriteInt32(buffer, EventPicker, 0x11c);
    context.WriteInt32(buffer, ComboIndex, 0x11c);
    context.WriteInt32(buffer, _prevComboIndex, 0x120);

    context.WriteInt32(buffer, Utilities::HashString(EventName), 0xdc);
    if (!EventName.empty())
    {
        context.WriteInt32(buffer, static_cast<int>(EventName.size()), 0xe0);
        context.WriteInt32(buffer, static_cast<int>(EventName.size() + 1), 0xe4);
    }
    context.WriteInt32(buffer, 1, 0xe8);
    context.WriteInt32(buffer, 0xBADF00D, 0xec);
    context.WriteInt32(buffer, 0xBADF00D, 0xf0);
}

int SeAudio_Wwise_Event_DefinitionNode::GetSizeForSerialization() const
{
    return 0x130;
}

} // namespace SlLib::Resources::Scene::Definitions
