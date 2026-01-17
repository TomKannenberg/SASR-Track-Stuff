#pragma once

#include <cstdint>
#include <vector>

namespace SlLib::Resources {

class SlConstantBuffer
{
public:
    SlConstantBuffer();
    ~SlConstantBuffer();

    std::vector<uint8_t> Data;
    uint32_t Size = 0;
};

} // namespace SlLib::Resources
