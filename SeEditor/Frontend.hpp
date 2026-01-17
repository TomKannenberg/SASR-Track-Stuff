#pragma once

#include <string>

namespace SeEditor::Editor {
class Scene;
}

namespace SeEditor {

class Frontend
{
public:
    Frontend(int width, int height);

    void LoadFrontend(std::string const& path);
    void SetActiveScene(Editor::Scene const& scene);

private:
    int _width;
    int _height;
};

} // namespace SeEditor
