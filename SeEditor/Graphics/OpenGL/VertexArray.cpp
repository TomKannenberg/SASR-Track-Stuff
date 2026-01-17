#include "VertexArray.hpp"

namespace SeEditor::Graphics::OpenGL {

void VertexArray::Bind()
{
    _bound = true;
}

void VertexArray::Unbind()
{
    _bound = false;
}

void VertexArray::Reset()
{
    _bound = false;
}

bool VertexArray::IsBound() const
{
    return _bound;
}

} // namespace SeEditor::Graphics::OpenGL
