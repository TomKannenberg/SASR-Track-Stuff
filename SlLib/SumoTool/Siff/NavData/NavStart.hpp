#pragma once

#include <memory>

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavTrackMarker;

namespace Serialization = SlLib::Serialization;

class NavStart final : public Serialization::IResourceSerializable
{
public:
    NavStart();
    ~NavStart();

    int DriveMode = 0;
    std::shared_ptr<NavTrackMarker> TrackMarker;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
