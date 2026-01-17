#pragma once

#include <array>

namespace SlLib::SumoTool::Siff::Forest {

class SuTextureTransform
{
public:
    SuTextureTransform();
    ~SuTextureTransform();

    std::array<float,4> Transform{};
};

} // namespace SlLib::SumoTool::Siff::Forest
