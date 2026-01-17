#pragma once

#include <cstdint>

namespace SlLib::Serialization::Streams {

class BinaryReaderBE
{
public:
    BinaryReaderBE() = default;
    ~BinaryReaderBE() = default;

    int32_t ReadInt32() const { return 0; }
};

} // namespace SlLib::Serialization::Streams
