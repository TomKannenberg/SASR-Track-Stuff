#include "SlResourceDatabase.hpp"

#include <algorithm>

namespace SlLib::Resources::Database {

SlResourceDatabase::SlResourceDatabase()
{
    Scene.SetNameWithTimestamp("scene_root");
}

SlLib::Resources::Scene::SeNodeBase* SlResourceDatabase::FindNode(int uid) const
{
    auto it = _nodeMap.find(uid);
    return it != _nodeMap.end() ? it->second : nullptr;
}

SlResourceChunk* SlResourceDatabase::FindChunkById(int id)
{
    auto it = std::find_if(Chunks.begin(), Chunks.end(), [id](SlResourceChunk const& chunk) { return chunk.Id == id; });
    return it != Chunks.end() ? &(*it) : nullptr;
}

SlResourceChunk* SlResourceDatabase::FindChunkByName(std::string const& name)
{
    auto it = std::find_if(Chunks.begin(), Chunks.end(), [&name](SlResourceChunk const& chunk) {
        return chunk.Name == name;
    });
    return it != Chunks.end() ? &(*it) : nullptr;
}

void SlResourceDatabase::AddOwnedNode(std::unique_ptr<SlLib::Resources::Scene::SeNodeBase> node)
{
    if (!node)
        return;

    _nodeMap[node->Uid] = node.get();
    _ownedNodes.push_back(std::move(node));
}

void SlResourceDatabase::RemoveNode(int uid)
{
    _nodeMap.erase(uid);
    _ownedNodes.erase(std::remove_if(_ownedNodes.begin(), _ownedNodes.end(),
        [uid](auto const& entry) {
            return entry->Uid == uid;
        }), _ownedNodes.end());
}

void SlResourceDatabase::ClearOwnedNodes()
{
    _nodeMap.clear();
    _ownedNodes.clear();
}

void SlResourceDatabase::AddDefinition(SlLib::Resources::Scene::SeDefinitionNode* definition)
{
    if (definition)
        RootDefinitions.push_back(definition);
}

void SlResourceDatabase::AddChunk(SlResourceChunk chunk)
{
    Chunks.push_back(std::move(chunk));
}

} // namespace SlLib::Resources::Database
