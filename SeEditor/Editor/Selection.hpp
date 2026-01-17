#pragma once

#include <vector>

namespace SlLib::Resources::Scene {
class SeGraphNode;
}

namespace SeEditor::Editor {

class Selection
{
public:
    static SlLib::Resources::Scene::SeGraphNode* ActiveNode;
    static std::vector<SlLib::Resources::Scene::SeGraphNode*> Clipboard;
};

} // namespace SeEditor::Editor
