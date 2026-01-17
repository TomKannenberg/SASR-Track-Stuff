#pragma once

#include <imgui.h>
#include <span>
#include <cstdint>

namespace SeEditor::Renderer {

class EditorFramebuffer final
{
public:
    EditorFramebuffer();
    ~EditorFramebuffer();

    void Initialize(int width, int height);
    void Bind() const;
    void Unbind() const;
    void Resize(int width, int height);
    bool IsValid() const;

    void UploadPixels(std::span<std::uint8_t const> data);
    ImTextureID GetTextureId() const;

private:
    void ResetTexture();

    std::uint32_t _texture = 0;
    int _width = 0;
    int _height = 0;
    bool _initialized = false;
};

} // namespace SeEditor::Renderer
