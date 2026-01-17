#include "CryptUtil.hpp"

#include <algorithm>
#include <stdexcept>

namespace SlLib::Utilities {

void CryptUtil::EncodeBuffer(std::span<std::uint8_t> buffer)
{
    const std::size_t length = buffer.size();
    if (length == 0) {
        return;
    }

    const std::size_t half = length / 2;
    if ((length & 1) != 0) {
        buffer[half] = static_cast<std::uint8_t>(buffer[half] + ShuffleKey[half % ShuffleKey.size()]);
    }

    for (std::size_t i = 0, j = length - 1; i < half; ++i, --j) {
        const std::uint8_t swap = static_cast<std::uint8_t>(buffer[i] + ShuffleKey[j % ShuffleKey.size()]);
        buffer[i] = static_cast<std::uint8_t>(buffer[j] + ShuffleKey[i % ShuffleKey.size()]);
        buffer[j] = swap;
    }

    for (std::size_t i = 0; i < half; ++i) {
        std::swap(buffer[i], buffer[i + half]);
    }
}

void CryptUtil::DecodeBuffer(std::span<std::uint8_t> buffer)
{
    const std::size_t length = buffer.size();
    if (length == 0) {
        return;
    }

    const std::size_t half = length / 2;
    for (std::size_t i = 0; i < half; ++i) {
        std::swap(buffer[i], buffer[i + half]);
    }

    std::size_t j = length - 1;
    for (std::size_t i = 0; i < half; ++i, --j) {
        const std::uint8_t swap = static_cast<std::uint8_t>(buffer[i] - ShuffleKey[i % ShuffleKey.size()]);
        buffer[i] = static_cast<std::uint8_t>(buffer[j] - ShuffleKey[j % ShuffleKey.size()]);
        buffer[j] = swap;
    }

    if ((length & 1) != 0) {
        const std::size_t middle = half;
        buffer[middle] = static_cast<std::uint8_t>(buffer[middle] - ShuffleKey[middle % ShuffleKey.size()]);
    }
}

void CryptUtil::PackFileUnmunge(std::span<std::uint8_t> buffer)
{
    if (buffer.size() < AndroidPackFileMungedIdentity.size()) {
        throw std::invalid_argument("TOC buffer must be at least 16 bytes to verify identity!");
    }

    if (!std::equal(buffer.begin(),
                    buffer.begin() + static_cast<std::ptrdiff_t>(AndroidPackFileMungedIdentity.size()),
                    AndroidPackFileMungedIdentity.begin())) {
        return;
    }

    for (std::size_t i = 0; i < buffer.size(); ++i) {
        const std::uint8_t key = PackFileMungeKey[i % PackFileMungeKey.size()];
        const std::uint8_t rotation = static_cast<std::uint8_t>((key << 4) | (key >> 4));
        buffer[i] = static_cast<std::uint8_t>(buffer[i] ^ rotation);
    }
}

} // namespace SlLib::Utilities
