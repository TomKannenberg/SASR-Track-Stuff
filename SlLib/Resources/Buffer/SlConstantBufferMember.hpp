#pragma once

#include <string>

namespace SlLib::Resources::Buffer {

struct SlConstantBufferMember
{
    int ArrayDataStride = 0;
    int ArrayElementStride = 0;
    int Columns = 1;
    int Components = 1;
    int Dimensions = 0;
    int MaxComponents = 4;
    std::string Name;
    int Offset = 0;
    int Rows = 1;
    int Size = 16;
};

} // namespace SlLib::Resources::Buffer
