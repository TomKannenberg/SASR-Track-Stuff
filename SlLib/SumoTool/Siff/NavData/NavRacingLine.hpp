#pragma once

#include <memory>
#include <vector>

#include "../../../Math/Vector.hpp"
#include "NavRacingLineSeg.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

namespace Serialization = SlLib::Serialization;

class NavRacingLine final : public Serialization::IResourceSerializable
{
public:
    NavRacingLine();
    ~NavRacingLine();

    std::vector<std::shared_ptr<NavRacingLineSeg>> Segments;
    bool Looping = false;
    int Permissions = 0x17;
    float TotalLength = 0.0f;
    float TotalBaseTime = 0.0f;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
