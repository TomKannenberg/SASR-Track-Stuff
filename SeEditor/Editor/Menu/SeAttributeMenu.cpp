#include "SeAttributeMenu.hpp"

#include "../../Graphics/ImGui/ImGuiHelper.hpp"
#include <SlLib/Resources/Scene/Instances/SeInstanceLightNode.hpp>
#include <imgui.h>

namespace {

constexpr auto toString(SlLib::Resources::Scene::Instances::SeInstanceLightNode::SeLightType type) noexcept
    -> const char*
{
    using SeLightType = SlLib::Resources::Scene::Instances::SeInstanceLightNode::SeLightType;
    switch (type)
    {
    case SeLightType::Ambient:
        return "Ambient";
    case SeLightType::Directional:
        return "Directional";
    case SeLightType::Point:
        return "Point";
    case SeLightType::Spot:
        return "Spot";
    }
    return "Unknown";
}

} // namespace

namespace SeEditor::Editor::Menu {

void SeAttributeMenu::Draw(SlLib::Resources::Scene::Instances::SeInstanceLightNode* node)
{
    if (node == nullptr)
        return;

    if (!ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
        return;

    ImGui::InputInt(Graphics::ImGui::DoLabelPrefix("Light Data Flags").c_str(), &node->LightDataFlags);

    ImGui::TextUnformatted(toString(node->LightType));

    ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Specular Multiplier").c_str(), &node->SpecularMultiplier, 0.01f);
    ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Intensity Multiplier").c_str(), &node->IntensityMultiplier, 0.01f);

    float color[3] = { node->Color.X, node->Color.Y, node->Color.Z };
    if (ImGui::ColorEdit3(Graphics::ImGui::DoLabelPrefix("Color").c_str(), color))
    {
        node->Color.X = color[0];
        node->Color.Y = color[1];
        node->Color.Z = color[2];
    }

    if (node->LightType == SlLib::Resources::Scene::Instances::SeInstanceLightNode::SeLightType::Point)
    {
        ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Inner Radius").c_str(), &node->InnerRadius, 0.01f);
        ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Outer Radius").c_str(), &node->OuterRadius, 0.01f);
        ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Falloff").c_str(), &node->Falloff, 0.01f);
    }
    else if (node->LightType == SlLib::Resources::Scene::Instances::SeInstanceLightNode::SeLightType::Spot)
    {
        ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Inner Cone Angle").c_str(), &node->InnerConeAngle, 0.01f);
        ImGui::DragFloat(Graphics::ImGui::DoLabelPrefix("Outer Cone Angle").c_str(), &node->OuterConeAngle, 0.01f);
    }
}

} // namespace SeEditor::Editor::Menu
