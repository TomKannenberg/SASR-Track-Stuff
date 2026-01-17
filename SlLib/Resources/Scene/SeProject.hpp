#pragma once

#include <memory>
#include <vector>

namespace SlLib::Serialization {
class ResourceLoadContext;
class ResourceSaveContext;
class ISaveBuffer;
}

namespace SlLib::Resources::Scene {
class SeDefinitionNode;
class SeInstanceNode;
}

namespace SlLib::Resources::Scene {

class SeProject
{
public:
    bool MasterProject = false;
    std::vector<std::unique_ptr<SeDefinitionNode>> Definitions;
    std::vector<std::unique_ptr<SeInstanceNode>> Instances;
    int EditState = 0;

    void Load(Serialization::ResourceLoadContext& context);
    void Save(Serialization::ResourceSaveContext& context, Serialization::ISaveBuffer& buffer);
};

} // namespace SlLib::Resources::Scene
