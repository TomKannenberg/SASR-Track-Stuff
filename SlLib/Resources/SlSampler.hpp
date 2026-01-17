#pragma once

#include <cstdint>

namespace SlLib::Resources {

class SlSampler
{
public:
    SlSampler();
    ~SlSampler();

    uint32_t FilterMode = 0;
    uint32_t AddressMode = 0;
};

} // namespace SlLib::Resources
