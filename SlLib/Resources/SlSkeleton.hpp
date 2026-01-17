#pragma once

#include "Skeleton/SlJoint.hpp"

#include <vector>

namespace SlLib::Resources {

class SlSkeleton
{
public:
    SlSkeleton();
    ~SlSkeleton();

    std::vector<Skeleton::SlJoint> Joints;
};

} // namespace SlLib::Resources
