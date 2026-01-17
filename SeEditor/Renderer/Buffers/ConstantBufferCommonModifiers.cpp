#include "ConstantBufferCommonModifiers.hpp"

namespace SeEditor::Renderer::Buffers {

void ConstantBufferCommonModifiers::SetAmbientColor(SlLib::Math::Vector4 color)
{
    _data.AmbientColor = color;
}

void ConstantBufferCommonModifiers::SetDiffuseColor(SlLib::Math::Vector4 color)
{
    _data.DiffuseColor = color;
}

void ConstantBufferCommonModifiers::SetExposure(float exposure)
{
    _data.Exposure = exposure;
}

void ConstantBufferCommonModifiers::SetBloom(float bloom)
{
    _data.Bloom = bloom;
}

ConstantBufferCommonModifiers::Data const& ConstantBufferCommonModifiers::GetData() const noexcept
{
    return _data;
}

} // namespace SeEditor::Renderer::Buffers
