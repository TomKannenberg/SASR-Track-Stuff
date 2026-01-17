#include "SlRenderBuffers.hpp"

#include <iostream>

namespace SeEditor::Renderer {

SlRenderBuffers::SlRenderBuffers() = default;

bool SlRenderBuffers::Initialize()
{
    if (_initialized)
        return true;

    _vao.Bind();
    _vertexBuffer.Bind();
    _indexBuffer.Bind();

    _initialized = true;
    std::cout << "SlRenderBuffers initialized." << std::endl;
    return true;
}

void SlRenderBuffers::Reset()
{
    if (!_initialized)
        return;

    _vao.Unbind();
    _vertexBuffer.Unbind();
    _indexBuffer.Unbind();
    _initialized = false;
}

bool SlRenderBuffers::IsInitialized() const noexcept
{
    return _initialized;
}

} // namespace SeEditor::Renderer
