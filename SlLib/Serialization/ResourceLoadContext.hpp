#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Database/SlResourceRelocation.hpp"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace SlLib::Resources::Database {
class SlPlatform;
class SlResourceDatabase;
}

namespace SlLib::Resources::Scene {
class SeNodeBase;
}

namespace SlLib::Serialization {

struct ISaveBuffer;

class ResourceLoadContext
{
public:
    ResourceLoadContext() = default;
    ResourceLoadContext(std::span<const std::uint8_t> data,
                        std::span<const std::uint8_t> gpuData = {},
                        std::vector<Resources::Database::SlResourceRelocation> relocations = {});

    int ReadInt32(std::size_t offset) const;
    int ReadInt32();

    float ReadFloat(std::size_t offset) const;
    float ReadFloat();

    int ReadBitset32(std::size_t offset) const;
    int ReadBitset32();

    std::string ReadStringPointer(std::size_t offset) const;
    std::string ReadStringPointer();

    Math::Matrix4x4 ReadMatrix(std::size_t offset) const;

    Math::Vector2 ReadFloat2(std::size_t offset) const;
    Math::Vector2 ReadFloat2();

    Math::Vector3 ReadFloat3(std::size_t offset) const;
    Math::Vector3 ReadFloat3();

    Math::Vector4 ReadFloat4(std::size_t offset) const;
    Math::Vector4 ReadFloat4();

    std::int8_t ReadInt8(std::size_t offset) const;
    std::int8_t ReadInt8();

    std::int16_t ReadInt16(std::size_t offset) const;
    std::int16_t ReadInt16();

    std::int64_t ReadInt64(std::size_t offset) const;
    std::int64_t ReadInt64();

    bool ReadBoolean(std::size_t offset, bool wide = false) const;
    bool ReadBoolean(bool wide = false);

    int ReadPointer(std::size_t offset, bool& gpu) const;
    int ReadPointer(std::size_t offset) const;
    int ReadPointer();
    int ReadPointer(bool& gpu);

    std::span<const std::uint8_t> LoadBuffer(int address, int size, bool gpu) const;
    SlLib::Resources::Scene::SeNodeBase* LoadNode(int id) const;

    template <typename T>
    std::unique_ptr<T> LoadPointer()
    {
        int address = ReadPointer();
        if (address == 0)
            return {};

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        auto obj = LoadUniqueReference<T>();
        Position = link;
        return obj;
    }

    template <typename T>
    std::shared_ptr<T> LoadSharedPointer()
    {
        int address = ReadPointer();
        if (address == 0)
            return {};

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        auto obj = LoadSharedReference<T>();
        Position = link;
        return obj;
    }

    template <typename T>
    std::vector<std::shared_ptr<T>> LoadPointerArray(int count)
    {
        std::vector<std::shared_ptr<T>> list;
        int address = ReadPointer();
        count = ClampCount(count, "LoadPointerArray");
        if (count <= 0 || address == 0)
            return list;

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        list.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
            list.push_back(LoadSharedPointer<T>());
        Position = link;
        return list;
    }

    template <typename T>
    std::vector<T> LoadArrayPointer(int count, T (*reader)(ResourceLoadContext&))
    {
        std::vector<T> list;
        int address = ReadPointer();
        count = ClampCount(count, "LoadArrayPointer(reader)");
        if (count <= 0 || address == 0)
            return list;

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        list.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
            list.push_back(reader(*this));
        Position = link;
        return list;
    }

    template <typename T>
    std::vector<std::shared_ptr<T>> LoadArrayPointer(int count)
    {
        std::vector<std::shared_ptr<T>> list;
        int address = ReadPointer();
        count = ClampCount(count, "LoadArrayPointer");
        if (count <= 0 || address == 0)
            return list;

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        list.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
            list.push_back(LoadSharedReference<T>());
        Position = link;
        return list;
    }

    template <typename T>
    std::vector<T> LoadArrayPointer(int count, T (ResourceLoadContext::*reader)())
    {
        std::vector<T> list;
        int address = ReadPointer();
        count = ClampCount(count, "LoadArrayPointer(member)");
        if (count <= 0 || address == 0)
            return list;

        std::size_t link = Position;
        Position = static_cast<std::size_t>(address);
        list.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i)
            list.push_back((this->*reader)());
        Position = link;
        return list;
    }

    template <typename T>
    std::shared_ptr<T> LoadSharedReference()
    {
        std::size_t start = Position;
        auto it = _references.find(start);
        if (it != _references.end())
            return std::static_pointer_cast<T>(it->second);

        auto obj = std::make_shared<T>();
        _references[start] = obj;
        obj->Load(*this);
        Position = start + static_cast<std::size_t>(obj->GetSizeForSerialization());
        return obj;
    }

    template <typename T>
    std::unique_ptr<T> LoadUniqueReference()
    {
        std::size_t start = Position;
        auto obj = std::make_unique<T>();
        obj->Load(*this);
        Position = start + static_cast<std::size_t>(obj->GetSizeForSerialization());
        return obj;
    }

    std::span<const std::uint8_t> Data() const { return _data; }
    std::span<const std::uint8_t> GpuData() const { return _gpuData; }

    Resources::Database::SlPlatform* Platform = nullptr;
    Resources::Database::SlResourceDatabase* Database = nullptr;
    int Version = 0;
    std::size_t Position = 0;
    int Base = 0;
    int GpuBase = 0;

private:
    static constexpr int kMaxLoadCount = 1000000;

    int ClampCount(int count, char const* label) const
    {
        if (count < 0 || count > kMaxLoadCount)
        {
            std::cerr << "[ResourceLoadContext] " << label
                      << " count out of range: " << count << std::endl;
            return 0;
        }
        return count;
    }

    std::span<const std::uint8_t> _data{};
    std::span<const std::uint8_t> _gpuData{};
    std::vector<Resources::Database::SlResourceRelocation> _relocations;
    std::unordered_map<std::size_t, std::shared_ptr<void>> _references;
};

} // namespace SlLib::Serialization
