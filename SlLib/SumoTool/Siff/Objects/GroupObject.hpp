#pragma once

#include "IObjectDef.hpp"

namespace SlLib::SumoTool::Siff::Objects {

class GroupObject : public IObjectDef
{
public:
    GroupObject();
    ~GroupObject() override;
};

} // namespace SlLib::SumoTool::Siff::Objects
