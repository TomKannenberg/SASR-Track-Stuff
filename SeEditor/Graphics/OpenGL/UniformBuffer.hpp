#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SeEditor::Graphics::OpenGL {

class UniformBuffer final
{
public:
    UniformBuffer(std::size_t size, std::uint32_t binding);

    void Bind() const;
    void Unbind() const;

    void SetData(std::vector<std::uint8_t> const& data);

private:
    std::size_t _size;
    std::uint32_t _binding;
    std::vector<std::uint8_t> _storage;
};

} // namespace SeEditor::Graphics::OpenGL
