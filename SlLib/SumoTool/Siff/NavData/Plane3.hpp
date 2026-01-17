#pragma once

#include "../../../Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::NavData {

namespace Serialization = SlLib::Serialization;

class Plane3 final : public Serialization::IResourceSerializable
{
public:
    Plane3();
    ~Plane3();

    SlLib::Math::Vector3 Normal{};
    float Const = 0.0f;

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::NavData
