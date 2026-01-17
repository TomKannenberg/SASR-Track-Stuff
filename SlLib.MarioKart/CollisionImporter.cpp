#include "CollisionImporter.hpp"

#include <iostream>

namespace SlLib::MarioKart {

CollisionImporter::CollisionImporter(std::string pathRoot)
    : _pathRoot(std::move(pathRoot))
{}

void CollisionImporter::ImportCollision(std::string const& name)
{
    std::cout << "[CollisionImporter] Importing collision " << name << " under " << _pathRoot << std::endl;
}

void CollisionImporter::SetScale(float scale)
{
    _scale = scale;
    std::cout << "[CollisionImporter] Collision scale set to " << _scale << std::endl;
}

} // namespace SlLib::MarioKart
