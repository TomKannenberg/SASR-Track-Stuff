#pragma once

#include "IEditorPanel.hpp"

namespace SlLib::Resources::Scene {
class SeNodeBase;
}

namespace SeEditor::Editor {
class Selection;
}

namespace SeEditor::Editor::Menu {
class NodeAttributesMenu;
}

namespace SeEditor::Editor::Panel {

class InspectorPanel final : public IEditorPanel
{
public:
    void OnImGuiRender() override;
    void OnSelectionChanged() override;

private:
    SlLib::Resources::Scene::SeNodeBase* _selected = nullptr;
};

} // namespace SeEditor::Editor::Panel
