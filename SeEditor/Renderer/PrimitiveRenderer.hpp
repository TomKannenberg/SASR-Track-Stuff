#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Renderer {

class PrimitiveRenderer final
{
public:
    static void BeginPrimitiveScene() noexcept;
    static void EndPrimitiveScene() noexcept;
    static void DrawLine(SlLib::Math::Vector3 const& from,
                         SlLib::Math::Vector3 const& to,
                         SlLib::Math::Vector3 const& color) noexcept;
    static void SetCamera(SlLib::Math::Matrix4x4 const& view, SlLib::Math::Matrix4x4 const& proj) noexcept;
};

} // namespace SeEditor::Renderer
