#pragma once

#include <vector>
#include <cstdint>

namespace SlLib::Serialization {

class Slab
{
public:
    Slab();
    ~Slab();

    std::vector<uint8_t> Buffer;
};

} // namespace SlLib::Serialization
