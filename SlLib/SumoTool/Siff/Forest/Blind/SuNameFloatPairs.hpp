#pragma once

#include <string>

namespace SlLib::SumoTool::Siff::Forest::Blind {

class SuNameFloatPairs
{
public:
    SuNameFloatPairs();
    ~SuNameFloatPairs();

    std::string Name;
    float Value = 0.0f;
};

} // namespace SlLib::SumoTool::Siff::Forest::Blind
