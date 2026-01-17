#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Renderer::Buffers {

class ConstantBufferCommonModifiers final
{
public:
    struct Data
    {
        SlLib::Math::Vector4 AmbientColor{0.1f, 0.1f, 0.1f, 1.0f};
        SlLib::Math::Vector4 DiffuseColor{1.0f, 1.0f, 1.0f, 1.0f};
        float Exposure = 1.0f;
        float Bloom = 1.0f;
    };

    ConstantBufferCommonModifiers() = default;

    void SetAmbientColor(SlLib::Math::Vector4 color);
    void SetDiffuseColor(SlLib::Math::Vector4 color);
    void SetExposure(float exposure);
    void SetBloom(float bloom);

    Data const& GetData() const noexcept;

private:
    Data _data;
};

} // namespace SeEditor::Renderer::Buffers
