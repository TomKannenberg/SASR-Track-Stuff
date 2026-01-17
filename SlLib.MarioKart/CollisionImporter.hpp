#pragma once

#include <string>

namespace SlLib::MarioKart {

class CollisionImporter
{
public:
    explicit CollisionImporter(std::string pathRoot);

    void ImportCollision(std::string const& name);
    void SetScale(float scale);

private:
    std::string _pathRoot;
    float _scale = 1.0f;
};

} // namespace SlLib::MarioKart
