#pragma once

#include <vector>

namespace SlLib::SumoTool::Siff::Forest {

class SuRenderVertexStream
{
public:
    SuRenderVertexStream();
    ~SuRenderVertexStream();

    std::vector<float> Vertices;
};

} // namespace SlLib::SumoTool::Siff::Forest
