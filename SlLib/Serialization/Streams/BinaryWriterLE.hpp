#pragma once

#include <cstdint>

namespace SlLib::Serialization::Streams {

class BinaryWriterLE
{
public:
    BinaryWriterLE() = default;
    ~BinaryWriterLE() = default;

    void WriteInt32(int32_t) {}
};

} // namespace SlLib::Serialization::Streams
