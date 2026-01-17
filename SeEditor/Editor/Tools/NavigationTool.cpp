#include "NavigationTool.hpp"

#include "../../Renderer/PrimitiveRenderer.hpp"

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Editor::Tools {

NavigationTool::NavigationTool(SlLib::SumoTool::Siff::Navigation* navigation) noexcept
    : _navData(navigation)
{}

void NavigationTool::OnRender()
{
    if (_navData == nullptr)
        return;

    Renderer::PrimitiveRenderer::BeginPrimitiveScene();
    for (auto const& line : _navData->RacingLines)
    {
        RenderRacingLine(line);
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
        auto* link = segment.Link;
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
            Renderer::PrimitiveRenderer::DrawLine(segment.RacingLine, next.RacingLine, white);
        }
    }
}

} // namespace SeEditor::Editor::Tools
