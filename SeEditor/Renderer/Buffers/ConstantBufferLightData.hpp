#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Renderer::Buffers {

class ConstantBufferLightData final
{
public:
    struct Data
    {
        SlLib::Math::Vector4 Direction{0.0f, -1.0f, 0.0f, 0.0f};
        SlLib::Math::Vector4 Color{1.0f, 1.0f, 1.0f, 1.0f};
        float Intensity = 1.0f;
    };

    ConstantBufferLightData() = default;

    void SetDirection(SlLib::Math::Vector4 direction);
    void SetColor(SlLib::Math::Vector4 color);
    void SetIntensity(float intensity);

    Data const& GetData() const noexcept;

private:
    Data _data;
};

} // namespace SeEditor::Renderer::Buffers
