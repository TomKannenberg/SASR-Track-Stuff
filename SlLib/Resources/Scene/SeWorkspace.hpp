#pragma once

#include <memory>
#include <vector>

#include "SeProject.hpp"

namespace SlLib::Serialization {
class ResourceLoadContext;
}

namespace SlLib::Resources::Scene {

class SeWorkspace
{
public:
    std::vector<std::unique_ptr<SeProject>> Projects;

    void Load(Serialization::ResourceLoadContext& context);
};

} // namespace SlLib::Resources::Scene
