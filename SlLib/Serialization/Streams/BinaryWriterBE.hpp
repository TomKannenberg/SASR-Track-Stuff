#pragma once

#include <cstdint>

namespace SlLib::Serialization::Streams {

class BinaryWriterBE
{
public:
    BinaryWriterBE() = default;
    ~BinaryWriterBE() = default;

    void WriteInt32(int32_t) {}
};

} // namespace SlLib::Serialization::Streams
