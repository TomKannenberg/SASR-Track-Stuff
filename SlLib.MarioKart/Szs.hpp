#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SlLib::MarioKart {

namespace detail {
extern "C" uint32_t riiszs_is_compressed(const uint8_t* src, uint32_t len);
extern "C" uint32_t riiszs_decoded_size(const uint8_t* src, uint32_t len);
extern "C" const char* riiszs_decode(uint8_t* dst, uint32_t dst_len, const uint8_t* src, uint32_t src_len);
extern "C" uint32_t riiszs_encoded_upper_bound(uint32_t len);
extern "C" const char* riiszs_encode_algo_fast(uint8_t* dst, uint32_t dst_len, const uint8_t* src, uint32_t src_len, uint32_t* used_len, uint32_t algo);
extern "C" void riiszs_free_error_message(const char* msg);
extern "C" int szs_get_version_unstable_api(uint8_t* buf, uint32_t len);
} // namespace detail

namespace szs {

enum class CompressionAlgorithm
{
    WorstCaseEncoding = 0,
    Nintendo,
    MkwSP,
    CTGP,
    Haroohie,
    CTlib,
    LibYaz0,
    MK8
};

bool IsCompressed(const std::vector<uint8_t>& data);
std::vector<uint8_t> Decode(const std::vector<uint8_t>& compressedData);
std::vector<uint8_t> Encode(const std::vector<uint8_t>& data, CompressionAlgorithm algorithm = CompressionAlgorithm::MK8);
std::string GetVersion();

} // namespace szs
} // namespace SlLib::MarioKart
