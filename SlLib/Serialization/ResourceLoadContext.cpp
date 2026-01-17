#include "ResourceLoadContext.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Resources/Database/SlResourceDatabase.hpp"
#include "SlLib/Resources/Database/SlResourceRelocation.hpp"
#include "SlLib/Resources/Scene/SeNodeBase.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

#include <algorithm>
#include <cstring>

namespace SlLib::Serialization {

namespace {

template <typename T>
T ByteSwap(T value)
{
    if constexpr (sizeof(T) == 2)
    {
        auto v = static_cast<std::uint16_t>(value);
        return static_cast<T>((v >> 8) | (v << 8));
    }
    else if constexpr (sizeof(T) == 4)
    {
        auto v = static_cast<std::uint32_t>(value);
        return static_cast<T>(((v & 0x000000FFu) << 24) |
                              ((v & 0x0000FF00u) << 8) |
                              ((v & 0x00FF0000u) >> 8) |
                              ((v & 0xFF000000u) >> 24));
    }
    else if constexpr (sizeof(T) == 8)
    {
        auto v = static_cast<std::uint64_t>(value);
        return static_cast<T>(((v & 0x00000000000000FFull) << 56) |
                              ((v & 0x000000000000FF00ull) << 40) |
                              ((v & 0x0000000000FF0000ull) << 24) |
                              ((v & 0x00000000FF000000ull) << 8) |
                              ((v & 0x000000FF00000000ull) >> 8) |
                              ((v & 0x0000FF0000000000ull) >> 24) |
                              ((v & 0x00FF000000000000ull) >> 40) |
                              ((v & 0xFF00000000000000ull) >> 56));
    }
    else
    {
        return value;
    }
}

bool IsBigEndian(Resources::Database::SlPlatform const* platform)
{
    return platform ? platform->IsBigEndian() : false;
}

int PointerSize(Resources::Database::SlPlatform const* platform)
{
    return platform && platform->Is64Bit() ? 8 : 4;
}

} // namespace

ResourceLoadContext::ResourceLoadContext(std::span<const std::uint8_t> data,
                                         std::span<const std::uint8_t> gpuData,
                                         std::vector<Resources::Database::SlResourceRelocation> relocations)
    : _data(data),
      _gpuData(gpuData),
      _relocations(std::move(relocations))
{
}

int ResourceLoadContext::ReadInt32(std::size_t offset) const
{
    if (offset + sizeof(std::int32_t) > _data.size())
        return 0;

    std::int32_t value = 0;
    std::memcpy(&value, _data.data() + offset, sizeof(value));
    if (IsBigEndian(Platform))
        value = ByteSwap(value);
    return value;
}

int ResourceLoadContext::ReadInt32()
{
    int value = ReadInt32(Position);
    Position += sizeof(std::int32_t);
    return value;
}

float ResourceLoadContext::ReadFloat(std::size_t offset) const
{
    if (offset + sizeof(float) > _data.size())
        return 0.0f;

    std::uint32_t raw = 0;
    std::memcpy(&raw, _data.data() + offset, sizeof(raw));
    if (IsBigEndian(Platform))
        raw = ByteSwap(raw);

    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

float ResourceLoadContext::ReadFloat()
{
    float value = ReadFloat(Position);
    Position += sizeof(float);
    return value;
}

int ResourceLoadContext::ReadBitset32(std::size_t offset) const
{
    int value = ReadInt32(offset);
    if (!IsBigEndian(Platform))
        return value;

    std::uint32_t v = static_cast<std::uint32_t>(value);
    std::uint32_t out = 0;
    for (int i = 0; i < 32; ++i)
        out |= ((v >> i) & 1u) << (31 - i);
    return static_cast<int>(out);
}

int ResourceLoadContext::ReadBitset32()
{
    int value = ReadBitset32(Position);
    Position += sizeof(std::int32_t);
    return value;
}

namespace {
std::string ReadStringAt(std::span<const std::uint8_t> data, std::size_t address)
{
    if (address >= data.size())
        return {};

    std::size_t cursor = address;
    std::string out;
    while (cursor < data.size())
    {
        char c = static_cast<char>(data[cursor++]);
        if (c == '\0')
            break;
        out.push_back(c);
    }
    return out;
}
} // namespace

std::string ResourceLoadContext::ReadStringPointer(std::size_t offset) const
{
    int address = ReadPointer(offset);
    if (address == 0)
        return {};

    return ReadStringAt(_data, static_cast<std::size_t>(address));
}

std::string ResourceLoadContext::ReadStringPointer()
{
    int address = ReadPointer();
    if (address == 0)
        return {};

    return ReadStringAt(_data, static_cast<std::size_t>(address));
}

Math::Matrix4x4 ResourceLoadContext::ReadMatrix(std::size_t offset) const
{
    Math::Matrix4x4 matrix{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            matrix(row, col) = ReadFloat(offset + (row * 16) + (col * 4));
    return matrix;
}

Math::Vector2 ResourceLoadContext::ReadFloat2(std::size_t offset) const
{
    return {ReadFloat(offset), ReadFloat(offset + 4)};
}

Math::Vector2 ResourceLoadContext::ReadFloat2()
{
    Math::Vector2 value = ReadFloat2(Position);
    Position += sizeof(float) * 2;
    return value;
}

Math::Vector3 ResourceLoadContext::ReadFloat3(std::size_t offset) const
{
    return {ReadFloat(offset), ReadFloat(offset + 4), ReadFloat(offset + 8)};
}

Math::Vector3 ResourceLoadContext::ReadFloat3()
{
    Math::Vector3 value = ReadFloat3(Position);
    Position += sizeof(float) * 3;
    return value;
}

Math::Vector4 ResourceLoadContext::ReadFloat4(std::size_t offset) const
{
    return {ReadFloat(offset), ReadFloat(offset + 4), ReadFloat(offset + 8), ReadFloat(offset + 12)};
}

Math::Vector4 ResourceLoadContext::ReadFloat4()
{
    Math::Vector4 value = ReadFloat4(Position);
    Position += sizeof(float) * 4;
    return value;
}

std::int8_t ResourceLoadContext::ReadInt8(std::size_t offset) const
{
    if (offset >= _data.size())
        return 0;
    return static_cast<std::int8_t>(_data[offset]);
}

std::int8_t ResourceLoadContext::ReadInt8()
{
    std::int8_t value = ReadInt8(Position);
    Position += sizeof(std::int8_t);
    return value;
}

std::int16_t ResourceLoadContext::ReadInt16(std::size_t offset) const
{
    if (offset + sizeof(std::int16_t) > _data.size())
        return 0;

    std::int16_t value = 0;
    std::memcpy(&value, _data.data() + offset, sizeof(value));
    if (IsBigEndian(Platform))
        value = ByteSwap(value);
    return value;
}

std::int16_t ResourceLoadContext::ReadInt16()
{
    std::int16_t value = ReadInt16(Position);
    Position += sizeof(std::int16_t);
    return value;
}

std::int64_t ResourceLoadContext::ReadInt64(std::size_t offset) const
{
    if (offset + sizeof(std::int64_t) > _data.size())
        return 0;

    std::int64_t value = 0;
    std::memcpy(&value, _data.data() + offset, sizeof(value));
    if (IsBigEndian(Platform))
        value = ByteSwap(value);
    return value;
}

std::int64_t ResourceLoadContext::ReadInt64()
{
    std::int64_t value = ReadInt64(Position);
    Position += sizeof(std::int64_t);
    return value;
}

bool ResourceLoadContext::ReadBoolean(std::size_t offset, bool wide) const
{
    if (wide)
        return ReadInt32(offset) != 0;
    if (offset >= _data.size())
        return false;
    return _data[offset] != 0;
}

bool ResourceLoadContext::ReadBoolean(bool wide)
{
    bool value = ReadBoolean(Position, wide);
    Position += wide ? sizeof(std::int32_t) : sizeof(std::uint8_t);
    return value;
}

int ResourceLoadContext::ReadPointer(std::size_t offset, bool& gpu) const
{
    gpu = false;

    int address = 0;
    if (PointerSize(Platform) == 8)
        address = static_cast<int>(ReadInt64(offset));
    else
        address = ReadInt32(offset);

    auto it = std::find_if(_relocations.begin(), _relocations.end(),
        [offset](auto const& rel) { return static_cast<std::size_t>(rel.Offset) == offset; });

    if (it != _relocations.end())
        gpu = it->IsGpuPointer();

    if (address == 0)
        return 0;

    return gpu ? (GpuBase + address) : (Base + address);
}

int ResourceLoadContext::ReadPointer(std::size_t offset) const
{
    bool gpu = false;
    return ReadPointer(offset, gpu);
}

int ResourceLoadContext::ReadPointer()
{
    int value = ReadPointer(Position);
    Position += static_cast<std::size_t>(PointerSize(Platform));
    return value;
}

int ResourceLoadContext::ReadPointer(bool& gpu)
{
    int value = ReadPointer(Position, gpu);
    Position += static_cast<std::size_t>(PointerSize(Platform));
    return value;
}

std::span<const std::uint8_t> ResourceLoadContext::LoadBuffer(int address, int size, bool gpu) const
{
    if (address < 0 || size <= 0)
        return {};

    std::size_t start = static_cast<std::size_t>(address);
    std::size_t end = start + static_cast<std::size_t>(size);
    if (gpu)
    {
        if (start >= _gpuData.size())
            return {};
        end = std::min(end, _gpuData.size());
        return _gpuData.subspan(start, end - start);
    }

    if (start >= _data.size())
        return {};
    end = std::min(end, _data.size());
    return _data.subspan(start, end - start);
}

SlLib::Resources::Scene::SeNodeBase* ResourceLoadContext::LoadNode(int id) const
{
    if (!Database || id == 0)
        return nullptr;

    if (Database->Scene.Uid == id)
        return &Database->Scene;

    return Database->FindNode(id);
}

} // namespace SlLib::Serialization
