#pragma once

#include <string>

namespace SlLib::Resources {

class SlModel
{
public:
    SlModel();
    ~SlModel();

    std::string FilePath;
    int VertexCount = 0;
};

} // namespace SlLib::Resources
