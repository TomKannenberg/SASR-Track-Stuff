#pragma once

#include <cstdint>
#include <string_view>

namespace SlLib::Utilities {
    std::int32_t sumoHash(std::string_view input);
}

namespace SlLib::Excel {

class CellType
{
public:
    static std::int32_t Float();
    static std::int32_t String();
    static std::int32_t Int();
    static std::int32_t Uint();
};

} // namespace SlLib::Excel
