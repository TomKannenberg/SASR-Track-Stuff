#pragma once

#include <string>

namespace SlLib::SumoTool::Siff::Objects {

class IObjectDef
{
public:
    virtual ~IObjectDef() = default;
    std::string Name;
};

} // namespace SlLib::SumoTool::Siff::Objects
