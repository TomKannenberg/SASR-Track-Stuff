#include "InspectorPanel.hpp"

#include "../Selection.hpp"
#include "../Menu/NodeAttributesMenu.hpp"
// ImGui helper includes
#include "../../Graphics/ImGui/ImGuiHelper.hpp"

#include <SlLib/Resources/Scene/SeNodeBase.hpp>
#include <SlLib/Resources/Scene/SeGraphNode.hpp>
#include <imgui.h>

#include <array>
#include <algorithm>
#include <cstring>
#include <string>

namespace SeEditor::Editor::Panel {
namespace Menu = SeEditor::Editor::Menu;

void InspectorPanel::OnImGuiRender()
{
    if (_selected == nullptr) {
        return;
    }

    bool isActive = (_selected->BaseFlags & 0x1) != 0;
    bool isVisible = (_selected->BaseFlags & 0x2) != 0;

    ImGui::Checkbox("###BaseNodeEnabledToggle", &isActive);
    ImGui::SameLine();

    std::array<char, 256> nameBuffer{};
    std::size_t nameLength = std::min(_selected->ShortName.size(), nameBuffer.size() - 1);
    std::memcpy(nameBuffer.data(), _selected->ShortName.c_str(), nameLength);

    ImGui::PushItemWidth(-1.0f);
    if (ImGui::InputText("##BaseNodeName", nameBuffer.data(), nameBuffer.size()))
    {
        _selected->ShortName = nameBuffer.data();
    }
    ImGui::PopItemWidth();

    std::array<char, 256> tagBuffer{};
    std::size_t tagLength = std::min(_selected->Tag.size(), tagBuffer.size() - 1);
    std::memcpy(tagBuffer.data(), _selected->Tag.c_str(), tagLength);

    std::string tagLabel = Graphics::ImGui::DoLabelPrefix("Tag");
    if (ImGui::InputText(tagLabel.c_str(), tagBuffer.data(), tagBuffer.size()))
    {
        _selected->Tag = tagBuffer.data();
    }

    Graphics::ImGui::DoLabelPrefix("Type");
    ImGui::TextUnformatted(_selected->DebugResourceType.c_str());

    std::string visibleLabel = Graphics::ImGui::DoLabelPrefix("Visible");
    ImGui::Checkbox(visibleLabel.c_str(), &isVisible);

    _selected->BaseFlags = (_selected->BaseFlags & ~1) | (isActive ? 1 : 0);
    _selected->BaseFlags = (_selected->BaseFlags & ~2) | (isVisible ? 2 : 0);

    Graphics::ImGui::DoBoldText("Attributes");
    Menu::NodeAttributesMenu::Draw(_selected);
}

void InspectorPanel::OnSelectionChanged()
{
    _selected = dynamic_cast<SlLib::Resources::Scene::SeNodeBase*>(Selection::ActiveNode);
}

} // namespace SeEditor::Editor::Panel
