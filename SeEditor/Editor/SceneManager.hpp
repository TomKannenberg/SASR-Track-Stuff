#pragma once

#include <memory>
#include <string>

namespace SeEditor::Editor {
class Scene;

class SceneManager final
{
public:
    static Scene* Current();
    static void LoadScene(std::string path, std::string extension = "pc");
    static void SetActiveScene(std::unique_ptr<Scene> scene);

    static bool RenderNavigationOnly;
    static bool DisableRendering;

private:
    static std::unique_ptr<Scene> s_current;
};

} // namespace SeEditor::Editor
