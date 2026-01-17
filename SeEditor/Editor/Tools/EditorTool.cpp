#include "EditorTool.hpp"

namespace SeEditor::Editor::Tools {

EditorTool::~EditorTool() = default;

void EditorTool::OnGUI()
{
    // Default tools do not render UI.
}

} // namespace SeEditor::Editor::Tools
