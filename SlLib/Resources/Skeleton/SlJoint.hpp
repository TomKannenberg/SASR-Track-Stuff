#pragma once

#include <string>

namespace SlLib::Resources::Skeleton {

class SlJoint
{
public:
    SlJoint();
    ~SlJoint();

    std::string Name;
    int ParentIndex = -1;
    float Weight = 1.0f;
};

} // namespace SlLib::Resources::Skeleton
