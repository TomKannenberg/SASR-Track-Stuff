#pragma once

#include <memory>
#include <string>

#include "SceneCamera.hpp"
#include "../../SlLib/Resources/Database/SlResourceDatabase.hpp"
#include "../../SlLib/SumoTool/Siff/Navigation.hpp"

namespace SeEditor::Editor {

class Scene final
{
public:
    Scene();

    SceneCamera Camera;
    SlLib::Resources::Database::SlResourceDatabase Database;
    std::unique_ptr<SlLib::SumoTool::Siff::Navigation> NavigationData;
    SlLib::SumoTool::Siff::Navigation* Navigation = nullptr;
    std::string SourceFileName;
};

} // namespace SeEditor::Editor
