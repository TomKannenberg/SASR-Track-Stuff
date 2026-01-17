#pragma once

#include <cstdint>
#include <vector>

namespace SeEditor::Graphics::OpenGL {

class IndexBuffer final
{
public:
    IndexBuffer() = default;

    void SetData(std::vector<std::uint32_t> const& indices);

    void Bind() const;
    void Unbind() const;

    [[nodiscard]] std::size_t Count() const;

private:
    std::vector<std::uint32_t> _indices;
};

} // namespace SeEditor::Graphics::OpenGL
