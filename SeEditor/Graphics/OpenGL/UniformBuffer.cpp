#include "UniformBuffer.hpp"

#include <algorithm>

namespace SeEditor::Graphics::OpenGL {

UniformBuffer::UniformBuffer(std::size_t size, std::uint32_t binding)
    : _size(size)
    , _binding(binding)
    , _storage(size)
{
}

void UniformBuffer::Bind() const
{
    // Placeholder: no actual GL binding happens yet.
}

void UniformBuffer::Unbind() const
{
    // Placeholder.
}

void UniformBuffer::SetData(std::vector<std::uint8_t> const& data)
{
    std::size_t copySize = std::min(data.size(), _storage.size());
    std::copy_n(data.data(), copySize, _storage.data());
}

} // namespace SeEditor::Graphics::OpenGL
