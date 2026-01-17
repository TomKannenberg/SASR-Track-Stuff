#pragma once

#include "SlResourceRelocation.hpp"
#include "SlResourceType.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace SlLib::Resources::Database {

struct SlResourceChunk
{
    SlResourceChunk() = default;

    SlResourceChunk(SlResourceType type, int id, std::string name, bool isResource)
        : Type(type)
        , Id(id)
        , Name(std::move(name))
        , IsResource(isResource)
    {
    }

    SlResourceType Type = SlResourceType::Invalid;
    int Version = 0;
    bool IsResource = false;
    int Id = 0;
    std::string Name;
    std::string Scene;
    std::vector<uint8_t> Data;
    std::vector<uint8_t> GpuData;
    std::vector<SlResourceRelocation> Relocations;
};

} // namespace SlLib::Resources::Database
