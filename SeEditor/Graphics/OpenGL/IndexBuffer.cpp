#include "IndexBuffer.hpp"

namespace SeEditor::Graphics::OpenGL {

void IndexBuffer::SetData(std::vector<std::uint32_t> const& indices)
{
    _indices = indices;
}

void IndexBuffer::Bind() const
{
    // Placeholder: actual GL binding will be implemented later.
}

void IndexBuffer::Unbind() const
{
    // Placeholder: actual GL unbinding will be implemented later.
}

std::size_t IndexBuffer::Count() const
{
    return _indices.size();
}

} // namespace SeEditor::Graphics::OpenGL
