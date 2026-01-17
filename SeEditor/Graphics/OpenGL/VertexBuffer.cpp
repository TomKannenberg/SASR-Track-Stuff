#include "VertexBuffer.hpp"

namespace SeEditor::Graphics::OpenGL {

void VertexBuffer::SetData(std::vector<std::uint8_t> const& data)
{
    _data = data;
}

void VertexBuffer::Bind()
{
    _bound = true;
}

void VertexBuffer::Unbind()
{
    _bound = false;
}

std::size_t VertexBuffer::Size() const
{
    return _data.size();
}

} // namespace SeEditor::Graphics::OpenGL
