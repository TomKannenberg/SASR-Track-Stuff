#pragma once

#include <vector>

namespace SlLib::SumoTool::Siff {

class SceneLibrary
{
public:
    SceneLibrary();
    ~SceneLibrary();

    std::vector<int> Scenes;
};

} // namespace SlLib::SumoTool::Siff
