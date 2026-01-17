#pragma once

#include <cstdint>
#include <vector>

namespace SlLib::Resources::Skeleton {

class JointHierarchyCacheItem
{
public:
    JointHierarchyCacheItem();
    ~JointHierarchyCacheItem();

    std::vector<int> JointOrder;
    std::vector<uint32_t> JointHashes;
};

} // namespace SlLib::Resources::Skeleton
