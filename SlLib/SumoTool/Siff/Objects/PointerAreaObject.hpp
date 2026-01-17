#pragma once

#include "IObjectDef.hpp"

namespace SlLib::SumoTool::Siff::Objects {

class PointerAreaObject : public IObjectDef
{
public:
    PointerAreaObject();
    ~PointerAreaObject() override;
};

} // namespace SlLib::SumoTool::Siff::Objects
