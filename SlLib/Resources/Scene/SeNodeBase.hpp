#pragma once

#include <string>

#include "SlLib/Serialization/IResourceSerializable.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

namespace SlLib::Utilities {
class SlUtil;
}

namespace SlLib::Resources::Scene {

class SeNodeBase : public Serialization::IResourceSerializable
{
public:
    SeNodeBase();

    std::string UidName;
    std::string ShortName;
    std::string Scene;
    std::string CleanName;
    std::string Tag;
    std::string DebugResourceType;
    int Uid = 0;
    int FileClassSize = 0;
    int BaseFlags = 0x2187;

    void SetNameWithTimestamp(std::string const& name);

    void Load(Serialization::ResourceLoadContext& context) override;
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer) override;
    int GetSizeForSerialization() const override;

protected:
    virtual int LoadInternal(Serialization::ResourceLoadContext& context, int offset);
};

} // namespace SlLib::Resources::Scene
