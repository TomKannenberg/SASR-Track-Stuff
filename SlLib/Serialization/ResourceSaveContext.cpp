#include "ResourceSaveContext.hpp"

#include "SlLib/Resources/Database/SlRelocationType.hpp"
#include "SlLib/Utilities/SlUtil.hpp"

#include <algorithm>
#include <cstring>

namespace SlLib::Serialization {

ResourceSaveContext::ResourceSaveContext() = default;

ResourceSaveContext::~ResourceSaveContext() = default;

ISaveBuffer ResourceSaveContext::Allocate(std::size_t size, int align, bool gpu, ISaveBuffer*)
{
    std::size_t address = AllocateAddress(size, align, gpu);
    auto storage = std::make_shared<std::vector<std::uint8_t>>(size);
    return ISaveBuffer(storage, 0, address, size);
}

ISaveBuffer ResourceSaveContext::SaveGenericPointer(ISaveBuffer& buffer, int offset, int size, int align)
{
    ISaveBuffer allocated = Allocate(static_cast<std::size_t>(size), align, false, &buffer);
    WriteInt32(buffer, static_cast<int>(allocated.Address), static_cast<std::size_t>(offset));
    Relocations.push_back(
        SlLib::Resources::Database::SlResourceRelocation{static_cast<int>(buffer.Address + offset),
                                                         static_cast<int>(SlLib::Resources::Database::SlRelocationType::Pointer)}
    );
    return allocated;
}

void ResourceSaveContext::SavePointer(ISaveBuffer& buffer, IResourceSerializable* writable, int offset, int align, bool)
{
    if (writable == nullptr)
        return;

    ISaveBuffer allocated = Allocate(static_cast<std::size_t>(writable->GetSizeForSerialization()), align, false, &buffer);
    writable->Save(*this, allocated);
    WriteInt32(buffer, static_cast<int>(allocated.Address), static_cast<std::size_t>(offset));
    Relocations.push_back(
        SlLib::Resources::Database::SlResourceRelocation{static_cast<int>(buffer.Address + offset),
                                                         static_cast<int>(SlLib::Resources::Database::SlRelocationType::Pointer)}
    );
}

void ResourceSaveContext::SaveBufferPointer(ISaveBuffer& buffer, std::span<std::uint8_t> data, int offset, int align, bool gpu)
{
    if (data.empty())
        return;

    ISaveBuffer allocated = Allocate(static_cast<std::size_t>(data.size()), align, gpu, &buffer);
    WriteBuffer(allocated, data, 0);
    WriteInt32(buffer, static_cast<int>(allocated.Address), static_cast<std::size_t>(offset));
    SlLib::Resources::Database::SlRelocationType type =
        gpu ? SlLib::Resources::Database::SlRelocationType::GpuPointer : SlLib::Resources::Database::SlRelocationType::Pointer;
    Relocations.push_back(SlLib::Resources::Database::SlResourceRelocation{
        static_cast<int>(buffer.Address + offset),
        static_cast<int>(type)});
}

void ResourceSaveContext::WriteInt32(ISaveBuffer& buffer, int value, std::size_t offset)
{
    EnsureCapacity(buffer, offset, sizeof(int));
    std::memcpy(buffer.Data() + offset, &value, sizeof(int));
}

void ResourceSaveContext::WriteFloat(ISaveBuffer& buffer, float value, std::size_t offset)
{
    EnsureCapacity(buffer, offset, sizeof(float));
    std::memcpy(buffer.Data() + offset, &value, sizeof(float));
}

void ResourceSaveContext::WriteFloat2(ISaveBuffer& buffer, Math::Vector2 const& value, std::size_t offset)
{
    WriteFloat(buffer, value.X, offset);
    WriteFloat(buffer, value.Y, offset + sizeof(float));
}

void ResourceSaveContext::WriteFloat3(ISaveBuffer& buffer, Math::Vector3 const& value, std::size_t offset)
{
    WriteFloat(buffer, value.X, offset);
    WriteFloat(buffer, value.Y, offset + sizeof(float));
    WriteFloat(buffer, value.Z, offset + sizeof(float) * 2);
}

void ResourceSaveContext::WriteFloat4(ISaveBuffer& buffer, Math::Vector4 const& value, std::size_t offset)
{
    WriteFloat(buffer, value.X, offset);
    WriteFloat(buffer, value.Y, offset + sizeof(float));
    WriteFloat(buffer, value.Z, offset + sizeof(float) * 2);
    WriteFloat(buffer, value.W, offset + sizeof(float) * 3);
}

void ResourceSaveContext::WriteMatrix(ISaveBuffer& buffer, Math::Matrix4x4 const& value, std::size_t offset)
{
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t col = 0; col < 4; ++col)
        {
            WriteFloat(buffer, value(row, col), offset + (row * 4 + col) * sizeof(float));
        }
    }
}

void ResourceSaveContext::WriteBoolean(ISaveBuffer& buffer, bool value, std::size_t offset, bool wide)
{
    if (wide)
    {
        WriteInt32(buffer, value ? 1 : 0, offset);
        return;
    }

    EnsureCapacity(buffer, offset, 1);
    buffer.Data()[offset] = value ? 1u : 0u;
}

void ResourceSaveContext::WriteStringPointer(ISaveBuffer& buffer, std::string const& value, std::size_t offset, bool allowEmptyString)
{
    if (value.empty())
    {
        if (allowEmptyString)
        {
            WriteString(buffer, value, 0);
            return;
        }

        WriteInt32(buffer, 0, offset);
        return;
    }

    ISaveBuffer allocated = Allocate(value.size() + 1, 1, false, &buffer);
    WriteString(allocated, value, 0);
    WriteInt32(buffer, static_cast<int>(allocated.Address), offset);
    Relocations.push_back(
        {static_cast<int>(buffer.Address + offset),
         static_cast<int>(SlLib::Resources::Database::SlRelocationType::Pointer)});
}

void ResourceSaveContext::WriteNodePointer(ISaveBuffer& buffer, SlLib::Resources::Scene::SeNodeBase* node, std::size_t offset)
{
    if (node == nullptr)
    {
        WriteInt32(buffer, 0, offset);
        return;
    }

    ISaveBuffer allocated = SaveGenericPointer(buffer, static_cast<int>(offset), 5, 1);
    WriteInt32(allocated, node->Uid, 0);
}

void ResourceSaveContext::WriteString(ISaveBuffer& buffer, std::string const& value, std::size_t offset)
{
    std::size_t total = value.size() + 1;
    EnsureCapacity(buffer, offset, total);
    std::memcpy(buffer.Data() + offset, value.data(), value.size());
    buffer.Data()[offset + value.size()] = '\0';
}

void ResourceSaveContext::WriteBuffer(ISaveBuffer& buffer, std::span<std::uint8_t const> data, std::size_t offset)
{
    if (data.empty())
        return;
    EnsureCapacity(buffer, offset, data.size());
    std::memcpy(buffer.Data() + offset, data.data(), data.size());
}

void ResourceSaveContext::WritePointerAtOffset(ISaveBuffer& buffer, int offset, int pointer)
{
    WriteInt32(buffer, pointer, static_cast<std::size_t>(offset));
    Relocations.push_back(
        {static_cast<int>(buffer.Address + offset),
         static_cast<int>(SlLib::Resources::Database::SlRelocationType::Pointer)});
}

void ResourceSaveContext::EnsureCapacity(ISaveBuffer& buffer, std::size_t offset, std::size_t size)
{
    if (!buffer.Storage)
        return;

    std::size_t required = buffer.Offset + offset + size;
    if (buffer.Storage->size() < required)
        buffer.Storage->resize(required);

    buffer.Size = buffer.Storage->size() - buffer.Offset;
}

std::size_t ResourceSaveContext::AlignAddress(std::size_t value, int align) const
{
    if (align <= 0)
        return value;
    return SlLib::Utilities::align(value, static_cast<std::size_t>(align));
}

std::size_t ResourceSaveContext::AllocateAddress(std::size_t size, int align, bool gpu)
{
    std::size_t& cursor = gpu ? _gpuSize : _cpuSize;
    std::size_t address = AlignAddress(cursor, align);
    cursor = address + size;
    return address;
}

} // namespace SlLib::Serialization
