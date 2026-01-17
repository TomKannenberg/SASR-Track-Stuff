#pragma once

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::Logic {

namespace Serialization = SlLib::Serialization;

class TriggerAttribute final : public Serialization::IResourceSerializable
{
public:
    TriggerAttribute();
    ~TriggerAttribute();

    int NameHash = 0;
    int PackedValue = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::Logic
