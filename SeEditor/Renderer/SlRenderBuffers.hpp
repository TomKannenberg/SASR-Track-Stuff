#pragma once

#include "EditorFramebuffer.hpp"

#include "../Graphics/OpenGL/IndexBuffer.hpp"
#include "../Graphics/OpenGL/VertexArray.hpp"
#include "../Graphics/OpenGL/VertexBuffer.hpp"

namespace SeEditor::Renderer {

class SlRenderBuffers final
{
public:
    SlRenderBuffers();

    bool Initialize();
    void Reset();
    bool IsInitialized() const noexcept;

private:
    Graphics::OpenGL::VertexArray _vao;
    Graphics::OpenGL::VertexBuffer _vertexBuffer;
    Graphics::OpenGL::IndexBuffer _indexBuffer;
    bool _initialized = false;
};

} // namespace SeEditor::Renderer
