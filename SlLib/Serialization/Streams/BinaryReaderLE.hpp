#pragma once

#include <cstdint>

namespace SlLib::Serialization::Streams {

class BinaryReaderLE
{
public:
    BinaryReaderLE() = default;
    ~BinaryReaderLE() = default;

    int32_t ReadInt32() const { return 0; }
};

} // namespace SlLib::Serialization::Streams
