#pragma once

#include <memory>

namespace SlLib::Resources::Scene {
class SeNodeBase;
}

namespace SeEditor::Editor::Menu {

class NodeAttributesMenu final
{
public:
    static void Draw(SlLib::Resources::Scene::SeNodeBase* node);
};

} // namespace SeEditor::Editor::Menu
