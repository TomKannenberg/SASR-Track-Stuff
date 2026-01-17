#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace SlLib::Serialization {

struct ISaveBuffer
{
    std::shared_ptr<std::vector<std::uint8_t>> Storage;
    std::size_t Offset = 0;
    std::size_t Position = 0;
    std::size_t Address = 0;
    std::size_t Size = 0;

    ISaveBuffer() = default;
    explicit ISaveBuffer(std::size_t size);
    ISaveBuffer(std::shared_ptr<std::vector<std::uint8_t>> storage, std::size_t offset,
                std::size_t address, std::size_t size);

    std::uint8_t* Data() const;
    std::size_t Capacity() const;
    ISaveBuffer At(int offset, int size) const;
};

} // namespace SlLib::Serialization
