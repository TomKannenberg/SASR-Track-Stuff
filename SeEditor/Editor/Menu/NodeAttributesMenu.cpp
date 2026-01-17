#include "NodeAttributesMenu.hpp"

#include "../../Graphics/ImGui/ImGuiHelper.hpp"
#include <imgui.h>

#include <SlLib/Enums/InstanceTimeStep.hpp>
#include <SlLib/Math/Vector.hpp>
#include <SlLib/Resources/Scene/SeDefinitionNode.hpp>
#include <SlLib/Resources/Scene/SeDefinitionTransformNode.hpp>
#include <SlLib/Resources/Scene/SeInstanceNode.hpp>
#include <SlLib/Resources/Scene/Instances/SeInstanceEntityNode.hpp>
#include <SlLib/Resources/Scene/Instances/SeInstanceTransformNode.hpp>

#include <array>
#include <string_view>

namespace SeEditor::Editor::Menu {

namespace {

auto ToVector4(SlLib::Math::Quaternion const& quaternion) -> SlLib::Math::Vector4
{
    return {quaternion.X, quaternion.Y, quaternion.Z, quaternion.W};
}

template <typename NodeT>
void DrawTransform(NodeT* node, std::string_view header)
{
    using namespace SeEditor::Graphics::ImGui;
    if (!StartPropertyTable(header))
        return;

    DrawDragFloat3("Translation", node->Translation);

    SlLib::Math::Vector4 rotation = ToVector4(node->Rotation);
    DrawDragFloat4("Rotation", rotation);
    node->Rotation.X = rotation.X;
    node->Rotation.Y = rotation.Y;
    node->Rotation.Z = rotation.Z;
    node->Rotation.W = rotation.W;

    DrawDragFloat3("Scale", node->Scale);
    DrawCheckboxFlags("Inherit Transforms", node->InheritTransforms, 1);

    EndPropertyTable();
}

void DrawInstance(SlLib::Resources::Scene::SeInstanceNode* instance)
{
    using namespace SeEditor::Graphics::ImGui;
    if (!StartPropertyTable("Instance"))
        return;

    DrawDragFloat("Local Time", instance->LocalTime);
    DrawDragFloat("Local Time Scale", instance->LocalTimeScale);

    int timestep = static_cast<int>(instance->TimeStep());
    if (DrawIndexedEnum("Time Frame", timestep))
        instance->SetTimeStep(static_cast<SlLib::Enums::InstanceTimeStep>(timestep));

    EndPropertyTable();
}

void DrawDefinition(SlLib::Resources::Scene::SeDefinitionNode* definition)
{
    using namespace SeEditor::Graphics::ImGui;
    if (!StartPropertyTable("Definition"))
        return;

    DoLabel("Instances");
    ImGui::Text("%zu", definition->Instances.size());

    EndPropertyTable();
}

void DrawEntity(SlLib::Resources::Scene::SeInstanceEntityNode* entity)
{
    using namespace SeEditor::Graphics::ImGui;
    if (!StartPropertyTable("Entity"))
        return;

    DrawInputInt("Render Layer", entity->RenderLayer);
    DrawInputInt("Transform Flags", entity->TransformFlags);

    EndPropertyTable();
}

} // namespace

void NodeAttributesMenu::Draw(SlLib::Resources::Scene::SeNodeBase* node)
{
    if (node == nullptr)
        return;

    using namespace SeEditor::Graphics::ImGui;

    if (auto* definition = dynamic_cast<SlLib::Resources::Scene::SeDefinitionNode*>(node))
        DrawDefinition(definition);

    if (auto* instance = dynamic_cast<SlLib::Resources::Scene::SeInstanceNode*>(node))
        DrawInstance(instance);

    if (auto* transform = dynamic_cast<SlLib::Resources::Scene::SeInstanceTransformNode*>(node))
        DrawTransform(transform, "Transform");

    if (auto* definitionTransform = dynamic_cast<SlLib::Resources::Scene::SeDefinitionTransformNode*>(node))
        DrawTransform(definitionTransform, "Definition Transform");

    if (auto* entity = dynamic_cast<SlLib::Resources::Scene::SeInstanceEntityNode*>(node))
        DrawEntity(entity);
}

} // namespace SeEditor::Editor::Menu
