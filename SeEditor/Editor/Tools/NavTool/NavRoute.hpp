#pragma once

#include <string>
#include <vector>

namespace SlLib::SumoTool::Siff::NavData {
class NavWaypoint;
}

namespace SeEditor::Editor::Tools::NavTool {

class NavRoute final
{
public:
    explicit NavRoute(int id);

    std::string Name;
    int Id;
    std::vector<SlLib::SumoTool::Siff::NavData::NavWaypoint*> Waypoints;
};

} // namespace SeEditor::Editor::Tools::NavTool
