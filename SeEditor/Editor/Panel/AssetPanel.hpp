#pragma once

#include "IEditorPanel.hpp"

namespace SeEditor::Editor::Panel {

class AssetPanel : public IEditorPanel
{
public:
    void OnImGuiRender() override;
};

} // namespace SeEditor::Editor::Panel
