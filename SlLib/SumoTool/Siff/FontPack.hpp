#pragma once

#include <string>
#include <vector>

namespace SlLib::SumoTool::Siff {

class FontPack
{
public:
    FontPack();
    ~FontPack();

    std::vector<std::string> Fonts;
};

} // namespace SlLib::SumoTool::Siff
