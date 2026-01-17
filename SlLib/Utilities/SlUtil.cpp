#include "SlUtil.hpp"

#include <cstdint>

namespace SlLib::Utilities {

int HashString(std::string_view input) noexcept
{
    constexpr uint32_t prime = 0x1000193u;
    uint32_t hash = 0x811c9dc5u;
    for (char c : input) {
        hash *= prime;
        hash ^= static_cast<uint8_t>(c);
    }
    return static_cast<int>(hash & 0xFFFFFFFFu);
}

} // namespace SlLib::Utilities
