#pragma once

namespace SeEditor::Graphics::OpenGL {

class VertexArray final
{
public:
    VertexArray() = default;

    void Bind();
    void Unbind();
    void Reset();

    bool IsBound() const;

private:
    bool _bound = false;
};

} // namespace SeEditor::Graphics::OpenGL
