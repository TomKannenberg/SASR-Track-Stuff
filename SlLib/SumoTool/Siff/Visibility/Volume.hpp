#pragma once

#include <array>

namespace SlLib::SumoTool::Siff::Visibility {

class Volume
{
public:
    Volume();
    ~Volume();

    std::array<float, 3> Size{};
};

} // namespace SlLib::SumoTool::Siff::Visibility
