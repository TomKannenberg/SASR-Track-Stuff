#include "NavRoute.hpp"

#include <string>

namespace SeEditor::Editor::Tools::NavTool {

NavRoute::NavRoute(int id)
    : Name("Route " + std::to_string(id))
    , Id(id)
{
}

} // namespace SeEditor::Editor::Tools::NavTool
