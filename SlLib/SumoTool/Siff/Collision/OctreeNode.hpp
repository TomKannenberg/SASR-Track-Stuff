#pragma once

#include <vector>

namespace SlLib::SumoTool::Siff::Collision {

class OctreeNode
{
public:
    OctreeNode();
    ~OctreeNode();

    std::vector<int> ChildIndices;
};

} // namespace SlLib::SumoTool::Siff::Collision
