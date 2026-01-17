#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace SeEditor::Graphics::OpenGL {

class Texture2D final
{
public:
    bool LoadFromFile(std::filesystem::path const& path);

    void Bind() const;
    void Unbind() const;

    [[nodiscard]] int Width() const;
    [[nodiscard]] int Height() const;

private:
    int _width = 0;
    int _height = 0;
    bool _loaded = false;
};

} // namespace SeEditor::Graphics::OpenGL
