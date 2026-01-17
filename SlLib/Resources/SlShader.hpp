#pragma once

#include <string>

namespace SlLib::Resources {

class SlShader
{
public:
    SlShader();
    ~SlShader();

    std::string VertexPath;
    std::string PixelPath;
};

} // namespace SlLib::Resources
