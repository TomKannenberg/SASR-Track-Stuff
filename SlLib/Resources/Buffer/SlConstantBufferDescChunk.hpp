#pragma once

#include "SlLib/Resources/Buffer/SlConstantBufferMember.hpp"

#include <string>
#include <vector>

namespace SlLib::Resources::Buffer {

struct SlConstantBufferDescChunk
{
    std::vector<SlConstantBufferMember> Members;
    std::string Name;
    int Size = 0;
};

} // namespace SlLib::Resources::Buffer
