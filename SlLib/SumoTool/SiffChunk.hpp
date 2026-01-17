#pragma once

#include <vector>

namespace SlLib::SumoTool::Siff {

class SiffChunk
{
public:
    SiffChunk();
    ~SiffChunk();

    std::vector<char> Data;
};

} // namespace SlLib::SumoTool::Siff
