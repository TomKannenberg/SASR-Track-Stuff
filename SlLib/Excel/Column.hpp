#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../Extensions/ByteReaderExtensions.hpp"
#include "SlLib/Utilities/SlUtil.hpp"
#include "Cell.hpp"
#include "CellType.hpp"

namespace SlLib::Excel {

class Column
{
public:
    std::vector<Cell> Cells;
    std::string Name;

    float GetFloat(std::string_view name, float fallback = 0.0f) const;
    int32_t GetInt(std::string_view name, int32_t fallback = 0) const;
    uint32_t GetUint(std::string_view name, uint32_t fallback = 0) const;
    std::string GetString(std::string_view name, std::string fallback = {}) const;

    static Column Load(std::span<const std::uint8_t> data, std::size_t offset);

private:
    const Cell* GetParamByName(int32_t hash) const;
    const Cell* GetParamByName(std::string_view name) const;
};

} // namespace SlLib::Excel
