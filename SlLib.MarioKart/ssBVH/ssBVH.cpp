#include "ssBVH.hpp"

#include <algorithm>

namespace SlLib::MarioKart::ssBVH {

void ssBVH::AddTriangle(ssBVH_Triangle triangle)
{
    Triangles.push_back(std::move(triangle));
}

void ssBVH::Build()
{
    Nodes.clear();
    if (Triangles.empty()) return;

    Nodes.reserve(Triangles.size() * 2);
    Nodes.push_back({});
    Nodes[0].Bounds = Triangles[0].Bounds();
    Nodes[0].TriangleIndex = 0;
    BuildNode(0);
}

void ssBVH::Clear()
{
    Nodes.clear();
    Triangles.clear();
}

void ssBVH::BuildNode(int nodeIndex)
{
    ssBVH_Node& node = Nodes[nodeIndex];
    if (node.TriangleIndex < 0 || node.TriangleIndex >= static_cast<int>(Triangles.size()))
        return;

    auto bounds = node.Bounds;
    for (int i = nodeIndex + 1; i < static_cast<int>(Triangles.size()); ++i)
    {
        const auto& tri = Triangles[i];
        bounds.Min.X = std::min(bounds.Min.X, tri.Bounds().Min.X);
        bounds.Min.Y = std::min(bounds.Min.Y, tri.Bounds().Min.Y);
        bounds.Min.Z = std::min(bounds.Min.Z, tri.Bounds().Min.Z);
        bounds.Max.X = std::max(bounds.Max.X, tri.Bounds().Max.X);
        bounds.Max.Y = std::max(bounds.Max.Y, tri.Bounds().Max.Y);
        bounds.Max.Z = std::max(bounds.Max.Z, tri.Bounds().Max.Z);
    }
    node.Bounds = bounds;
}

int ssBVH::AllocateNode()
{
    Nodes.emplace_back();
    return static_cast<int>(Nodes.size()) - 1;
}

} // namespace SlLib::MarioKart::ssBVH
