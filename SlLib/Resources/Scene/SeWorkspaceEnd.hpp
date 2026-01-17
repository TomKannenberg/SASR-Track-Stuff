#pragma once

#include "SeWorkspace.hpp"

namespace SlLib::Resources::Scene {

class SeWorkspaceEnd : public SeWorkspace
{
public:
    int GetSizeForSerialization() const;
};

} // namespace SlLib::Resources::Scene
