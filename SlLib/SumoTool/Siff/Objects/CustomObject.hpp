#pragma once

#include "IObjectDef.hpp"

namespace SlLib::SumoTool::Siff::Objects {

class CustomObject : public IObjectDef
{
public:
    CustomObject();
    ~CustomObject() override;
};

} // namespace SlLib::SumoTool::Siff::Objects
