#include "ConstantBufferViewProjection.hpp"

namespace {

SlLib::Math::Matrix4x4 multiply(SlLib::Math::Matrix4x4 const& lhs, SlLib::Math::Matrix4x4 const& rhs)
{
    SlLib::Math::Matrix4x4 result{};
    for (std::size_t row = 0; row < 4; ++row)
    {
        for (std::size_t col = 0; col < 4; ++col)
        {
            float sum = 0.0f;
            for (std::size_t idx = 0; idx < 4; ++idx)
                sum += lhs(row, idx) * rhs(idx, col);
            result(row, col) = sum;
        }
    }
    return result;
}

SlLib::Math::Matrix4x4 identityMatrix()
{
    SlLib::Math::Matrix4x4 matrix{};
    matrix(0, 0) = 1.0f;
    matrix(1, 1) = 1.0f;
    matrix(2, 2) = 1.0f;
    matrix(3, 3) = 1.0f;
    return matrix;
}

} // namespace

namespace SeEditor::Renderer::Buffers {

void ConstantBufferViewProjection::Update(SlLib::Math::Matrix4x4 const& viewMatrix,
                                          SlLib::Math::Matrix4x4 const& projectionMatrix)
{
    View = viewMatrix;
    Projection = projectionMatrix;
    ViewInverse = viewMatrix;
    ViewProjection = multiply(View, Projection);
}

void ConstantBufferViewProjection::Reset()
{
    View = identityMatrix();
    Projection = identityMatrix();
    ViewInverse = identityMatrix();
    ViewProjection = identityMatrix();
}

} // namespace SeEditor::Renderer::Buffers
