#include "ISaveBuffer.hpp"

#include <algorithm>

namespace SlLib::Serialization {

ISaveBuffer::ISaveBuffer(std::size_t size)
    : Storage(std::make_shared<std::vector<std::uint8_t>>(size))
    , Position(0)
    , Address(0)
    , Size(size)
{}

ISaveBuffer::ISaveBuffer(std::shared_ptr<std::vector<std::uint8_t>> storage,
                         std::size_t offset,
                         std::size_t address,
                         std::size_t size)
    : Storage(std::move(storage))
    , Offset(offset)
    , Position(address)
    , Address(address)
    , Size(size)
{}

std::uint8_t* ISaveBuffer::Data() const
{
    if (!Storage)
        return nullptr;
    return Storage->data() + Offset;
}

std::size_t ISaveBuffer::Capacity() const
{
    if (!Storage)
        return 0;
    return Storage->size() - Offset;
}

ISaveBuffer ISaveBuffer::At(int offset, int size) const
{
    ISaveBuffer buffer;
    buffer.Storage = Storage;
    buffer.Offset = Offset + static_cast<std::size_t>(offset);
    buffer.Address = Address + static_cast<std::size_t>(offset);
    buffer.Position = buffer.Address;
    buffer.Size = static_cast<std::size_t>(size);
    return buffer;
}

} // namespace SlLib::Serialization
