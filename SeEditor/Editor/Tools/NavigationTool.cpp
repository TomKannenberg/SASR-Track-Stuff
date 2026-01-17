#include "NavigationTool.hpp"

#include "../../Renderer/PrimitiveRenderer.hpp"

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Editor::Tools {

NavigationTool::NavigationTool(SlLib::SumoTool::Siff::Navigation* navigation) noexcept
    : _navData(navigation)
{}

void NavigationTool::SetRacingLineVisibility(std::vector<std::uint8_t> visibility)
{
    _racingLineVisibility = std::move(visibility);
}

void NavigationTool::SetDrawWaypoints(bool enabled)
{
    _drawWaypoints = enabled;
}

void NavigationTool::SetWaypointBoxSize(float size)
{
    _waypointBoxSize = size;
}

void NavigationTool::OnRender()
{
    if (_navData == nullptr)
        return;

    Renderer::PrimitiveRenderer::BeginPrimitiveScene();
    if (_drawWaypoints)
    {
        static const SlLib::Math::Vector3 waypointColor{1.0f, 0.0f, 0.0f};
        float half = std::max(0.1f, _waypointBoxSize * 0.5f);
        for (auto const& waypoint : _navData->Waypoints)
        {
            if (!waypoint)
                continue;
            SlLib::Math::Vector3 c = waypoint->Pos;
            SlLib::Math::Vector3 v0{c.X - half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v1{c.X + half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v2{c.X + half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v3{c.X - half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v4{c.X - half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v5{c.X + half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v6{c.X + half, c.Y + half, c.Z + half};
            SlLib::Math::Vector3 v7{c.X - half, c.Y + half, c.Z + half};

            Renderer::PrimitiveRenderer::DrawLine(v0, v1, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v1, v2, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v2, v3, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v3, v0, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v4, v5, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v5, v6, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v6, v7, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v7, v4, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v0, v4, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v1, v5, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v2, v6, waypointColor);
            Renderer::PrimitiveRenderer::DrawLine(v3, v7, waypointColor);
        }
    }
    for (std::size_t i = 0; i < _navData->RacingLines.size(); ++i)
    {
        auto const& line = _navData->RacingLines[i];
        if (!line)
            continue;
        if (!_racingLineVisibility.empty())
        {
            if (i >= _racingLineVisibility.size() || _racingLineVisibility[i] == 0)
                continue;
        }
        RenderRacingLine(*line);
    }
    Renderer::PrimitiveRenderer::EndPrimitiveScene();
}

void NavigationTool::RenderRacingLine(SlLib::SumoTool::Siff::NavData::NavRacingLine const& line)
{
    using SlLib::Math::Vector3;

    static const Vector3 markerColor{209.0f / 255.0f, 209.0f / 255.0f, 14.0f / 255.0f};
    static const Vector3 crossSectionColor{14.0f / 255.0f, 14.0f / 255.0f, 228.0f / 255.0f};
    static const Vector3 linkColor{215.0f / 255.0f, 14.0f / 255.0f, 255.0f / 255.0f};
    static const Vector3 white{1.0f, 1.0f, 1.0f};

    const auto& segments = line.Segments;
    if (segments.empty())
        return;

    for (std::size_t i = 0; i < segments.size(); ++i)
    {
        auto const& segment = segments[i];
        if (!segment)
            continue;
        auto* link = segment->Link.get();
        if (link == nullptr)
            continue;

        if (link->From != nullptr && link->To != nullptr)
            Renderer::PrimitiveRenderer::DrawLine(link->From->Pos, link->To->Pos, white);

        Renderer::PrimitiveRenderer::DrawLine(link->Left, link->Right, crossSectionColor);

        if (!link->CrossSection.empty())
        {
            for (std::size_t entry = 1; entry < link->CrossSection.size(); ++entry)
            {
                Renderer::PrimitiveRenderer::DrawLine(link->CrossSection[entry - 1],
                                                     link->CrossSection[entry],
                                                     linkColor);
            }
        }

        if (link->From != nullptr)
        {
            Renderer::PrimitiveRenderer::DrawLine(
                link->From->Pos,
                link->From->Pos + link->FromToNormal * 4.0f,
                markerColor);
        }

        if (segments.size() > 1)
        {
            auto const& next = segments[(i + 1) % segments.size()];
            if (next)
                Renderer::PrimitiveRenderer::DrawLine(segment->RacingLine, next->RacingLine, white);
        }
    }
}

} // namespace SeEditor::Editor::Tools
