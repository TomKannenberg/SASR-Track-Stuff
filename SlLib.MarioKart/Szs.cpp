#include "Szs.hpp"

#include <array>
#include <stdexcept>

namespace SlLib::MarioKart::szs {

bool IsCompressed(const std::vector<uint8_t>& data)
{
    if (data.empty())
        return false;

    return detail::riiszs_is_compressed(data.data(), static_cast<uint32_t>(data.size())) != 0;
}

std::vector<uint8_t> Decode(const std::vector<uint8_t>& compressedData)
{
    if (compressedData.empty())
        throw std::invalid_argument("Compressed data cannot be empty.");

    if (!IsCompressed(compressedData))
        throw std::invalid_argument("Data is not SZS (\"YAZ0\" or \"YAZ1\") compressed.");

    uint32_t expanded = detail::riiszs_decoded_size(compressedData.data(), static_cast<uint32_t>(compressedData.size()));
    std::vector<uint8_t> decodedData(expanded);

    const char* error = detail::riiszs_decode(decodedData.data(), expanded, compressedData.data(),
                                               static_cast<uint32_t>(compressedData.size()));
    if (error != nullptr)
    {
        std::string message(error);
        detail::riiszs_free_error_message(error);
        throw std::runtime_error("Encoding failed: " + message);
    }

    return decodedData;
}

std::vector<uint8_t> Encode(const std::vector<uint8_t>& data, CompressionAlgorithm algorithm)
{
    if (data.empty())
        throw std::invalid_argument("Input data cannot be empty.");

    uint32_t upperBound = detail::riiszs_encoded_upper_bound(static_cast<uint32_t>(data.size()));
    std::vector<uint8_t> encodedBuffer(upperBound);
    uint32_t usedLen = 0;

    const char* error = detail::riiszs_encode_algo_fast(encodedBuffer.data(), upperBound, data.data(),
                                                        static_cast<uint32_t>(data.size()), &usedLen,
                                                        static_cast<uint32_t>(algorithm));
    if (error != nullptr)
    {
        std::string message(error);
        detail::riiszs_free_error_message(error);
        throw std::runtime_error("Encoding failed: " + message);
    }

    encodedBuffer.resize(usedLen);
    return encodedBuffer;
}

std::string GetVersion()
{
    std::array<uint8_t, 256> buffer{};
    int length = detail::szs_get_version_unstable_api(buffer.data(), static_cast<uint32_t>(buffer.size()));
    if (length > 0 && length < static_cast<int>(buffer.size()))
        return std::string(reinterpret_cast<char*>(buffer.data()), static_cast<std::size_t>(length));

    throw std::runtime_error("Failed to retrieve version.");
}

} // namespace SlLib::MarioKart::szs
