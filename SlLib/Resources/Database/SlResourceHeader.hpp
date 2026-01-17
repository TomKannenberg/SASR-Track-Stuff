#pragma once

#include <string>
#include <string_view>

#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;
}

namespace SlLib::Resources::Database {
class SlPlatform;

class SlResourceHeader final : public Serialization::IResourceSerializable
{
public:
    SlPlatform* Platform = nullptr;
    int Id = 0;
    std::string Name;
    int Ref = 1;

    void Load(Serialization::ResourceLoadContext& context);
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer);
    int GetSizeForSerialization() const;

    void SetName(std::string_view tag);
};

} // namespace SlLib::Resources::Database
