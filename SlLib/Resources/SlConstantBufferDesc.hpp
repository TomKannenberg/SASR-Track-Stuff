#pragma once

#include <string>

namespace SlLib::Resources {

class SlConstantBufferDesc
{
public:
    SlConstantBufferDesc();
    ~SlConstantBufferDesc();

    std::string Name;
    int RegisterSlot = 0;
};

} // namespace SlLib::Resources
