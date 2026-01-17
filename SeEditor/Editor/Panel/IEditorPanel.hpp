#pragma once

namespace SeEditor::Editor::Panel {

class IEditorPanel
{
public:
    virtual ~IEditorPanel();

    /// Called every render frame.
    virtual void OnImGuiRender() = 0;

    /// Called whenever the editor selection changes.
    virtual void OnSelectionChanged();

    /// Called each update tick.
    virtual void OnUpdate();
};

} // namespace SeEditor::Editor::Panel
