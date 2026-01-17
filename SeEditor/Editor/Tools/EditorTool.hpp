#pragma once

#include <cstddef>

namespace SlLib::Resources::Scene {
class SeGraphNode;
}

namespace SeEditor::Editor::Tools {

class EditorTool
{
public:
    virtual ~EditorTool();

    virtual void OnRender() = 0;
    virtual void OnGUI();

    SlLib::Resources::Scene::SeGraphNode* Target = nullptr;
};

} // namespace SeEditor::Editor::Tools
