#pragma once

#include "SlLib/Resources/Scene/SeDefinitionNode.hpp"
#include "SlLib/Resources/Scene/Instances/SeInstanceSceneNode.hpp"
#include "SlLib/Resources/Scene/SeNodeBase.hpp"
#include "SlResourceChunk.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace SlLib::Resources::Database {

class SlResourceDatabase
{
public:
    SlResourceDatabase();
    SlResourceDatabase(SlResourceDatabase const&) = delete;
    SlResourceDatabase& operator=(SlResourceDatabase const&) = delete;
    SlResourceDatabase(SlResourceDatabase&&) = default;
    SlResourceDatabase& operator=(SlResourceDatabase&&) = default;

    [[nodiscard]] SlLib::Resources::Scene::SeNodeBase* FindNode(int uid) const;
    [[nodiscard]] SlResourceChunk* FindChunkById(int id);
    [[nodiscard]] SlResourceChunk* FindChunkByName(std::string const& name);

    void AddOwnedNode(std::unique_ptr<SlLib::Resources::Scene::SeNodeBase> node);
    void RemoveNode(int uid);
    void ClearOwnedNodes();

    void AddDefinition(SlLib::Resources::Scene::SeDefinitionNode* definition);
    void AddChunk(SlResourceChunk chunk);

    SlLib::Resources::Scene::SeInstanceSceneNode Scene;
    std::vector<SlLib::Resources::Scene::SeDefinitionNode*> RootDefinitions;
    std::vector<SlResourceChunk> Chunks;

private:
    std::unordered_map<int, SlLib::Resources::Scene::SeNodeBase*> _nodeMap;
    std::vector<std::unique_ptr<SlLib::Resources::Scene::SeNodeBase>> _ownedNodes;
};

} // namespace SlLib::Resources::Database
