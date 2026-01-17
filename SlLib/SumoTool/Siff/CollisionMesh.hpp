#pragma once

#include <vector>

#include "Collision/Triangle.hpp"

namespace SlLib::SumoTool::Siff::Collision {

class CollisionMesh
{
public:
    CollisionMesh();
    ~CollisionMesh();

    std::vector<Triangle> Triangles;
};

} // namespace SlLib::SumoTool::Siff::Collision
