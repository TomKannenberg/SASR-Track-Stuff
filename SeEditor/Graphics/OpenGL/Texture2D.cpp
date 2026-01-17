#include "Texture2D.hpp"

#include <filesystem>
#include <iostream>

namespace SeEditor::Graphics::OpenGL {

bool Texture2D::LoadFromFile(std::filesystem::path const& path)
{
    if (!std::filesystem::exists(path))
    {
        std::cerr << "Texture not found: " << path << std::endl;
        return false;
    }

    _width = 1;
    _height = 1;
    _loaded = true;
    return true;
}

void Texture2D::Bind() const
{
    // Placeholder: binding not implemented yet.
}

void Texture2D::Unbind() const
{
    // Placeholder: unbinding not implemented yet.
}

int Texture2D::Width() const
{
    return _width;
}

int Texture2D::Height() const
{
    return _height;
}

} // namespace SeEditor::Graphics::OpenGL
