#pragma once

#include <array>

namespace SlLib::SumoTool::Siff::Collision {

class OctreeChildIndices
{
public:
    OctreeChildIndices();
    ~OctreeChildIndices();

    std::array<int, 8> Indices;
};

} // namespace SlLib::SumoTool::Siff::Collision
