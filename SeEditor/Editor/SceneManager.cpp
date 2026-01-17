#include "SceneManager.hpp"

#include "Scene.hpp"
#include "SceneCamera.hpp"

#include "../Managers/SlFile.hpp"
#include "Selection.hpp"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

namespace SeEditor::Editor {

std::unique_ptr<Scene> SceneManager::s_current = std::make_unique<Scene>();
bool SceneManager::RenderNavigationOnly = false;
bool SceneManager::DisableRendering = false;

Scene* SceneManager::Current()
{
    return s_current.get();
}

void SceneManager::LoadScene(std::string path, std::string extension)
{
    auto database = SlFile::GetSceneDatabase(path, extension);
    if (!database)
        throw std::runtime_error("Unable to load scene database: " + path);

    auto scene = std::make_unique<Scene>();
    scene->SourceFileName = path;
    scene->Database = std::move(*database);

    std::string navPath = path + ".nav" + extension;
    if (SlFile::DoesFileExist(navPath))
    {
        auto navData = SlFile::GetFile(navPath);
        if (navData)
            std::cout << "Navigation data loaded (parsing deferred)." << std::endl;
        else
            std::cout << "Failed to read navigation data for " << navPath << std::endl;
    }
    else
    {
        std::cout << "Navigation file missing: " << navPath << std::endl;
    }

    scene->NavigationData = std::make_unique<SlLib::SumoTool::Siff::Navigation>();
    scene->NavigationData->GenerateTestRoute();
    scene->Navigation = scene->NavigationData.get();

    SetActiveScene(std::move(scene));
}

void SceneManager::SetActiveScene(std::unique_ptr<Scene> scene)
{
    if (!scene)
        return;

    s_current = std::move(scene);
    SceneCamera::SetActive(s_current->Camera);
    Selection::ActiveNode = nullptr;
    Selection::Clipboard.clear();
}

} // namespace SeEditor::Editor
