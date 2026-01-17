#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Database/SlRelocationType.hpp"
#include "SlLib/Resources/Database/SlResourceRelocation.hpp"
#include "SlLib/Resources/Scene/SeNodeBase.hpp"
#include "SlLib/Serialization/ISaveBuffer.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace SlLib::Serialization {
class IResourceSerializable;
class ISaveBuffer;
}

namespace SlLib::Serialization {

class ResourceSaveContext
{
public:
    ResourceSaveContext();
    ~ResourceSaveContext();

    ISaveBuffer Allocate(std::size_t size, int align = 4, bool gpu = false, ISaveBuffer* parentSaveBuffer = nullptr);
    ISaveBuffer SaveGenericPointer(ISaveBuffer& buffer, int offset, int size, int align = 4);
    void SavePointer(ISaveBuffer& buffer, IResourceSerializable* writable, int offset, int align = 4, bool deferred = false);
    void SaveBufferPointer(ISaveBuffer& buffer, std::span<std::uint8_t> data, int offset, int align = 4, bool gpu = false);
    void SaveReference(ISaveBuffer&, IResourceSerializable*, int) {}
    void SaveObject(ISaveBuffer&, IResourceSerializable*, int) {}

    template <typename Container>
    void SaveReferenceArray(ISaveBuffer&, Container const&, int, int = 4)
    {
    }

    template <typename Container>
    void SavePointerArray(ISaveBuffer&, Container const&, int, int = 4, int = 4, bool = false)
    {
    }

    template <typename Container>
    void SaveObjectArray(ISaveBuffer&, Container const&, int, int = 4)
    {
    }

    void WriteInt32(ISaveBuffer& buffer, int value, std::size_t offset);
    void WriteFloat(ISaveBuffer& buffer, float value, std::size_t offset);
    void WriteFloat2(ISaveBuffer& buffer, Math::Vector2 const& value, std::size_t offset);
    void WriteFloat3(ISaveBuffer& buffer, Math::Vector3 const& value, std::size_t offset);
    void WriteFloat4(ISaveBuffer& buffer, Math::Vector4 const& value, std::size_t offset);
    void WriteMatrix(ISaveBuffer& buffer, Math::Matrix4x4 const& value, std::size_t offset);
    void WriteBoolean(ISaveBuffer& buffer, bool value, std::size_t offset, bool wide = false);
    void WriteStringPointer(ISaveBuffer& buffer, std::string const& value, std::size_t offset,
                            bool allowEmptyString = false);
    void WriteNodePointer(ISaveBuffer& buffer, SlLib::Resources::Scene::SeNodeBase* node, std::size_t offset);
    void WriteString(ISaveBuffer& buffer, std::string const& value, std::size_t offset);
    void WriteBuffer(ISaveBuffer& buffer, std::span<std::uint8_t const> data, std::size_t offset);

    void WritePointerAtOffset(ISaveBuffer& buffer, int offset, int pointer);

    int Version = 0;
    std::vector<SlLib::Resources::Database::SlResourceRelocation> Relocations;

private:
    void EnsureCapacity(ISaveBuffer& buffer, std::size_t offset, std::size_t size);
    std::size_t AlignAddress(std::size_t value, int align) const;
    std::size_t AllocateAddress(std::size_t size, int align, bool gpu);

    std::size_t _cpuSize = 0;
    std::size_t _gpuSize = 0;
};

} // namespace SlLib::Serialization
