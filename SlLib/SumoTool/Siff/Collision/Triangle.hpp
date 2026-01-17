#pragma once

#include <array>

namespace SlLib::SumoTool::Siff::Collision {

class Triangle
{
public:
    Triangle();
    ~Triangle();

    std::array<float, 3> Points[3];
};

} // namespace SlLib::SumoTool::Siff::Collision
