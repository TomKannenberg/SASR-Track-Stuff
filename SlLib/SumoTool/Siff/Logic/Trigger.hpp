#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::SumoTool::Siff::Logic {

namespace Serialization = SlLib::Serialization;

class Trigger final : public Serialization::IResourceSerializable
{
public:
    Trigger();
    ~Trigger();

    int NameHash = 0;
    int NumAttributes = 0;
    int AttributeStartIndex = 0;
    int Flags = 0;
    SlLib::Math::Vector4 Position{};
    SlLib::Math::Vector4 Normal{};
    SlLib::Math::Vector4 Vertex0{};
    SlLib::Math::Vector4 Vertex1{};
    SlLib::Math::Vector4 Vertex2{};
    SlLib::Math::Vector4 Vertex3{};

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;
};

} // namespace SlLib::SumoTool::Siff::Logic
