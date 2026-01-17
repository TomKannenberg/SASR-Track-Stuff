#pragma once

#include <vector>

namespace SlLib::SumoTool::Siff {

class ObjectDefLibrary
{
public:
    ObjectDefLibrary();
    ~ObjectDefLibrary();

    std::vector<int> Entries;
};

} // namespace SlLib::SumoTool::Siff
