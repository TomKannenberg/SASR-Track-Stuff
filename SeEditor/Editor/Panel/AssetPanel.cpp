#include "AssetPanel.hpp"

#include "../SceneManager.hpp"
#include "../Scene.hpp"
#include "../Selection.hpp"
#include <SlLib/Resources/Scene/SeDefinitionNode.hpp>
#include <imgui.h>

#include <string>

namespace SeEditor::Editor::Panel {

void AssetPanel::OnImGuiRender()
{
    auto* scene = SceneManager::Current();
    if (scene == nullptr)
    {
        ImGui::TextDisabled("No workspace loaded.");
        return;
    }

    const char* title = scene->SourceFileName.empty() ? "Unnamed Workspace" : scene->SourceFileName.c_str();
    ImGui::Text("Scene: %s", title);
    ImGui::Separator();

    auto const& definitions = scene->Database.RootDefinitions;
    if (definitions.empty())
    {
        ImGui::TextDisabled("No definitions available.");
        return;
    }

    for (auto* definition : definitions)
    {
        if (definition == nullptr)
            continue;

        std::string label = definition->ShortName.empty() ? "Unnamed Definition" : definition->ShortName;
        bool selected = Selection::ActiveNode == definition;
        if (ImGui::Selectable(label.c_str(), selected))
        {
            Selection::ActiveNode = definition;
        }
    }
}

} // namespace SeEditor::Editor::Panel
