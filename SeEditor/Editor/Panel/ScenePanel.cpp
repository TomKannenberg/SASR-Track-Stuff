#include "ScenePanel.hpp"

#include "../SceneManager.hpp"
#include "../Scene.hpp"

#include <imgui.h>

namespace SeEditor::Editor::Panel {

void ScenePanel::OnImGuiRender()
{
    auto* scene = SceneManager::Current();
    if (scene == nullptr)
    {
        ImGui::TextDisabled("No workspace loaded.");
        return;
    }

    ImGui::Text("Scene: %s", scene->SourceFileName.empty() ? "Unnamed Workspace" : scene->SourceFileName.c_str());
    ImGui::Separator();
    ImGui::Text("Navigation data: %s", scene->Navigation != nullptr ? "Available" : "Missing");

    ImVec2 area = ImGui::GetContentRegionAvail();
    ImGui::Dummy(ImVec2(area.x, area.y - 30.0f));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + area.x * 0.5f - 60.0f);
    ImGui::TextDisabled("[ Scene viewport placeholder ]");
}

} // namespace SeEditor::Editor::Panel
