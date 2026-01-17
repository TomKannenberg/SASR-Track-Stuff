#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "../Extensions/ByteReaderExtensions.hpp"
#include "Column.hpp"

namespace SlLib::Excel {

class Worksheet
{
public:
    std::vector<Column> Columns;
    std::string Name;

    Column* GetColumnByName(std::string_view name);
    const Column* GetColumnByName(std::string_view name) const;

    static Worksheet Load(std::span<const std::uint8_t> data, std::size_t offset);
};

} // namespace SlLib::Excel
