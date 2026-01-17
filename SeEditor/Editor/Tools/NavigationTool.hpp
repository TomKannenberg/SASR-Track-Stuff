#pragma once

#include "EditorTool.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/SumoTool/Siff/Navigation.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavRacingLine.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavWaypointLink.hpp>

#include <cstddef>

namespace SeEditor::Renderer {
class PrimitiveRenderer;
}

namespace SeEditor::Editor::Tools {

class NavigationTool final : public EditorTool
{
public:
    explicit NavigationTool(SlLib::SumoTool::Siff::Navigation* navigation) noexcept;

    void OnRender() override;

private:
    void RenderRacingLine(SlLib::SumoTool::Siff::NavData::NavRacingLine const& line);

    SlLib::SumoTool::Siff::Navigation* _navData;
};

} // namespace SeEditor::Editor::Tools
