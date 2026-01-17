#include "Selection.hpp"

namespace SeEditor::Editor {

SlLib::Resources::Scene::SeGraphNode* Selection::ActiveNode = nullptr;
std::vector<SlLib::Resources::Scene::SeGraphNode*> Selection::Clipboard = {};

} // namespace SeEditor::Editor
