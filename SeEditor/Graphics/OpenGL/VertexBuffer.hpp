#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SeEditor::Graphics::OpenGL {

class VertexBuffer final
{
public:
    VertexBuffer() = default;

    void SetData(std::vector<std::uint8_t> const& data);
    void Bind();
    void Unbind();

    [[nodiscard]] std::size_t Size() const;

private:
    std::vector<std::uint8_t> _data;
    bool _bound = false;
};

} // namespace SeEditor::Graphics::OpenGL
