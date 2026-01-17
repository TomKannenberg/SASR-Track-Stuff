#pragma once

#include <string>
#include <cstdint>

namespace SlLib::SumoTool::Siff::Forest::Blind {

class SuNameUint32Pairs
{
public:
    SuNameUint32Pairs();
    ~SuNameUint32Pairs();

    std::string Name;
    uint32_t Value = 0;
};

} // namespace SlLib::SumoTool::Siff::Forest::Blind
