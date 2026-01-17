#include "Frontend.hpp"

#include <iostream>

namespace SeEditor {

Frontend::Frontend(int width, int height)
    : _width(width)
    , _height(height)
{
    std::cout << "Frontend created (" << _width << "x" << _height << ")" << std::endl;
}

void Frontend::LoadFrontend(std::string const& path)
{
    std::cout << "Loading frontend package: " << path << std::endl;
}

void Frontend::SetActiveScene(Editor::Scene const&)
{
    // Placeholder: front-end scene activation will be implemented later.
}

} // namespace SeEditor
