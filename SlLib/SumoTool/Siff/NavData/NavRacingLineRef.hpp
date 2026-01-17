#pragma once

#include <memory>

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavRacingLine;

namespace Serialization = SlLib::Serialization;

class NavRacingLineRef final : public Serialization::IResourceSerializable
{
public:
    NavRacingLineRef();
    ~NavRacingLineRef();

    std::shared_ptr<NavRacingLine> RacingLine;
    int SegmentIndex = 0;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
