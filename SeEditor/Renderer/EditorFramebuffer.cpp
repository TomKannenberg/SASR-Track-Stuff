#include "EditorFramebuffer.hpp"

#include <iostream>

namespace SeEditor::Renderer {

EditorFramebuffer::EditorFramebuffer() = default;
EditorFramebuffer::~EditorFramebuffer() = default;

void EditorFramebuffer::Initialize(int width, int height)
{
    _width = width;
    _height = height;
    _initialized = true;
}

void EditorFramebuffer::Bind() const
{
    if (!_initialized)
        return;

}

void EditorFramebuffer::Unbind() const
{
    if (!_initialized)
        return;

}

void EditorFramebuffer::Resize(int width, int height)
{
    if (width == _width && height == _height)
        return;

    Initialize(width, height);
}

bool EditorFramebuffer::IsValid() const
{
    return _initialized;
}

} // namespace SeEditor::Renderer
