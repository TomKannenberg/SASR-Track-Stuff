#pragma once

#include <memory>
#include <vector>

#include "SlLib/Serialization/IResourceSerializable.hpp"
#include "Logic/Trigger.hpp"
#include "Logic/TriggerAttribute.hpp"
#include "Logic/Locator.hpp"

namespace SlLib::SumoTool::Siff {

namespace Serialization = SlLib::Serialization;

class LogicData final : public Serialization::IResourceSerializable
{
public:
    LogicData();
    ~LogicData();

    int NameHash = 0;
    int LogicVersion = 0;
    std::vector<std::shared_ptr<Logic::Trigger>> Triggers;
    std::vector<std::shared_ptr<Logic::TriggerAttribute>> Attributes;
    std::vector<std::shared_ptr<Logic::Locator>> Locators;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff
