#pragma once

#include "ssBVH_Node.hpp"
#include "ssBVH_Triangle.hpp"

#include <vector>

namespace SlLib::MarioKart::ssBVH {

class ssBVH
{
public:
    ssBVH() = default;

    void AddTriangle(ssBVH_Triangle triangle);
    void Build();
    void Clear();

    std::vector<ssBVH_Node> Nodes;
    std::vector<ssBVH_Triangle> Triangles;

private:
    void BuildNode(int nodeIndex);
    int AllocateNode();
};

} // namespace SlLib::MarioKart::ssBVH
