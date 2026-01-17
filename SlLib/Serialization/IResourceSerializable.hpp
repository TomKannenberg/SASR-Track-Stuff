#pragma once

namespace SlLib::Serialization {

class ResourceLoadContext;
class ResourceSaveContext;
struct ISaveBuffer;

class IResourceSerializable
{
public:
    virtual ~IResourceSerializable() = default;
    virtual void Load(ResourceLoadContext& context) = 0;
    virtual void Save(ResourceSaveContext& context, ISaveBuffer& buffer) = 0;
    virtual int GetSizeForSerialization() const = 0;
};

} // namespace SlLib::Serialization
