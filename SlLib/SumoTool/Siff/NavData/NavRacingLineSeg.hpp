#pragma once

#include <memory>

#include "../../../Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavWaypointLink;

namespace Serialization = SlLib::Serialization;

class NavRacingLineSeg final : public Serialization::IResourceSerializable
{
public:
    NavRacingLineSeg();
    ~NavRacingLineSeg();

    SlLib::Math::Vector3 RacingLine{};
    SlLib::Math::Vector3 SafeRacingLine{};
    float RacingLineScalar = 0.5f;
    float SafeRacingLineScalar = 0.5f;
    std::shared_ptr<NavWaypointLink> Link;
    float RacingLineLength = 1.0f;
    int TurnType = 0;
    float SmoothSideLeft = 0.0f;
    float SmoothSideRight = 0.0f;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
