#pragma once

#include "EditorTool.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/SumoTool/Siff/Navigation.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavRacingLine.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavWaypointLink.hpp>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SeEditor::Renderer {
class PrimitiveRenderer;
}

namespace SeEditor::Editor::Tools {

class NavigationTool final : public EditorTool
{
public:
    explicit NavigationTool(SlLib::SumoTool::Siff::Navigation* navigation) noexcept;

    void OnRender() override;
    void SetRacingLineVisibility(std::vector<std::uint8_t> visibility);
    void SetDrawWaypoints(bool enabled);
    void SetWaypointBoxSize(float size);

private:
    void RenderRacingLine(SlLib::SumoTool::Siff::NavData::NavRacingLine const& line);

    SlLib::SumoTool::Siff::Navigation* _navData;
    std::vector<std::uint8_t> _racingLineVisibility;
    bool _drawWaypoints = false;
    float _waypointBoxSize = 1.0f;
};

} // namespace SeEditor::Editor::Tools
