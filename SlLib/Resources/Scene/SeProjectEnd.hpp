#pragma once

#include "SeProject.hpp"

namespace SlLib::Resources::Scene {

class SeProjectEnd : public SeProject
{
public:
    void Load(Serialization::ResourceLoadContext& context);
};

} // namespace SlLib::Resources::Scene
