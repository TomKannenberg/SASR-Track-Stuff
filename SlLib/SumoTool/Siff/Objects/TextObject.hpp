#pragma once

#include "IObjectDef.hpp"

namespace SlLib::SumoTool::Siff::Objects {

class TextObject : public IObjectDef
{
public:
    TextObject();
    ~TextObject() override;
};

} // namespace SlLib::SumoTool::Siff::Objects
