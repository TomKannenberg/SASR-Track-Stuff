#pragma once

#include "IObjectDef.hpp"

namespace SlLib::SumoTool::Siff::Objects {

class GouraudObject : public IObjectDef
{
public:
    GouraudObject();
    ~GouraudObject() override;
};

} // namespace SlLib::SumoTool::Siff::Objects
