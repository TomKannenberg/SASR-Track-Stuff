#include "LogicData.hpp"

#include "SlLib/Serialization/ResourceLoadContext.hpp"
#include "SlLib/Serialization/ResourceSaveContext.hpp"

#include <algorithm>

namespace SlLib::SumoTool::Siff {

LogicData::LogicData() = default;
LogicData::~LogicData() = default;

void LogicData::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    NameHash = context.ReadInt32();
    LogicVersion = context.ReadInt32();

    int numTriggers = context.ReadInt32();
    int numLocators = context.ReadInt32();

    Triggers = context.LoadArrayPointer<Logic::Trigger>(numTriggers);

    int numAttributes = 0;
    for (auto const& trigger : Triggers)
    {
        if (!trigger)
            continue;
        numAttributes = std::max(numAttributes,
                                 trigger->AttributeStartIndex + trigger->NumAttributes);
    }
    Attributes = context.LoadArrayPointer<Logic::TriggerAttribute>(numAttributes);

    Locators = context.LoadArrayPointer<Logic::Locator>(numLocators);
}

void LogicData::Save(SlLib::Serialization::ResourceSaveContext&,
                     SlLib::Serialization::ISaveBuffer&)
{
}

int LogicData::GetSizeForSerialization() const
{
    return 0x20;
}

} // namespace SlLib::SumoTool::Siff
