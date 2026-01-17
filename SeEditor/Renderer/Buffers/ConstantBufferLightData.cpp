#include "ConstantBufferLightData.hpp"

namespace SeEditor::Renderer::Buffers {

void ConstantBufferLightData::SetDirection(SlLib::Math::Vector4 direction)
{
    _data.Direction = direction;
}

void ConstantBufferLightData::SetColor(SlLib::Math::Vector4 color)
{
    _data.Color = color;
}

void ConstantBufferLightData::SetIntensity(float intensity)
{
    _data.Intensity = intensity;
}

ConstantBufferLightData::Data const& ConstantBufferLightData::GetData() const noexcept
{
    return _data;
}

} // namespace SeEditor::Renderer::Buffers
