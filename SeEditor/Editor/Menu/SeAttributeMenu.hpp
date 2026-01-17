#pragma once

namespace SlLib::Resources::Scene::Instances {
class SeInstanceLightNode;
}

namespace SeEditor::Editor::Menu {

class SeAttributeMenu final
{
public:
    static void Draw(SlLib::Resources::Scene::Instances::SeInstanceLightNode* node);
};

} // namespace SeEditor::Editor::Menu
