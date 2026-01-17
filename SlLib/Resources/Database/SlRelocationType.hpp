#pragma once

#include <cstdint>

namespace SlLib::Resources::Database {

enum class SlRelocationType : std::int32_t
{
    Pointer = 0,
    Null = 1,
    Resource = 2,
    ResourcePair = 3,
    GpuPointer = 16
};

} // namespace SlLib::Resources::Database
