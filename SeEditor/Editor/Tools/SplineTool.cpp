#include "SplineTool.hpp"

#include "../../Renderer/PrimitiveRenderer.hpp"

#include <cstddef>

namespace SeEditor::Editor::Tools {
using SeEditor::Renderer::PrimitiveRenderer;

SlLib::Math::Vector3 SeInstanceSplineNode::ReadFloat3(std::size_t byteOffset) const
{
    const std::size_t floatIndex = byteOffset / sizeof(float);
    if (floatIndex + 2 >= Data.size())
        return {};

    return {Data[floatIndex], Data[floatIndex + 1], Data[floatIndex + 2]};
}

void SplineTool::OnRender()
{
    auto spline = dynamic_cast<SeInstanceSplineNode*>(Target);
    if (spline == nullptr || spline->Data.empty())
        return;

    const SlLib::Math::Vector3 pivot{
        spline->WorldMatrix(0, 3),
        spline->WorldMatrix(1, 3),
        spline->WorldMatrix(2, 3),
    };

    const std::size_t stepBytes = 0x40;
    const std::size_t dataBytes = spline->Data.size() * sizeof(float);
    if (dataBytes <= 0x30)
        return;

    auto translatePoint = [](SlLib::Math::Vector3 const& center, SlLib::Math::Vector3 const& point) {
        return SlLib::Math::Vector3{center.X + point.X, center.Y + point.Y, center.Z + point.Z};
    };

    PrimitiveRenderer::BeginPrimitiveScene();
    for (std::size_t offsetBytes = 0; offsetBytes < dataBytes; offsetBytes += stepBytes)
    {
        if (offsetBytes + 0x30 >= dataBytes)
            break;

        const auto position = translatePoint(pivot, spline->ReadFloat3(offsetBytes + 0x30));
        const auto nextOffset = ((offsetBytes + stepBytes) % dataBytes);
        const auto nextPosition = translatePoint(pivot, spline->ReadFloat3(nextOffset + 0x30));

        PrimitiveRenderer::DrawLine(position, nextPosition, {1.0f, 1.0f, 1.0f});
    }
    PrimitiveRenderer::EndPrimitiveScene();
}

} // namespace SeEditor::Editor::Tools
