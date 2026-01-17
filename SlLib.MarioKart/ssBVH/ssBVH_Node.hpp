#pragma once

#include "SSAABB.hpp"

namespace SlLib::MarioKart::ssBVH {

struct ssBVH_Node
{
    SSAABB Bounds{};
    int LeftIndex = -1;
    int RightIndex = -1;
    int TriangleIndex = -1;

    bool IsLeaf() const { return TriangleIndex >= 0; }
};

} // namespace SlLib::MarioKart::ssBVH
