#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "../Extensions/ByteReaderExtensions.hpp"
#include "Worksheet.hpp"

namespace SlLib::Excel {

class ExcelData
{
public:
    std::vector<Worksheet> Worksheets;

    Worksheet* GetWorksheet(std::string_view name);
    const Worksheet* GetWorksheet(std::string_view name) const;

    static ExcelData Load(std::span<const std::uint8_t> data);
    std::vector<std::uint8_t> Save() const;
};

} // namespace SlLib::Excel
