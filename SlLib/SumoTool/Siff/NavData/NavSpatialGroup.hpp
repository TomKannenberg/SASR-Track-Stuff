#pragma once

#include <memory>
#include <vector>

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypointLink;

namespace Serialization = SlLib::Serialization;

class NavSpatialGroup final : public Serialization::IResourceSerializable
{
public:
    NavSpatialGroup();
    ~NavSpatialGroup();

    std::vector<std::shared_ptr<NavWaypointLink>> Links;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
