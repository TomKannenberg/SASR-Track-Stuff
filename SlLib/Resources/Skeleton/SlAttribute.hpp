#pragma once

#include <string>

namespace SlLib::Resources::Skeleton {

class SlAttribute
{
public:
    SlAttribute();
    ~SlAttribute();

    std::string Name;
    std::string Value;
};

} // namespace SlLib::Resources::Skeleton
