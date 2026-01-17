#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Renderer::Buffers {

struct ConstantBufferViewProjection
{
    SlLib::Math::Matrix4x4 View{};
    SlLib::Math::Matrix4x4 Projection{};
    SlLib::Math::Matrix4x4 ViewInverse{};
    SlLib::Math::Matrix4x4 ViewProjection{};

    void Update(SlLib::Math::Matrix4x4 const& viewMatrix, SlLib::Math::Matrix4x4 const& projectionMatrix);
    void Reset();
};

} // namespace SeEditor::Renderer::Buffers
