#include "ExcelData.hpp"

#include <algorithm>
#include <stdexcept>

namespace SlLib::Excel {

Worksheet* ExcelData::GetWorksheet(std::string_view name)
{
    auto it = std::find_if(Worksheets.begin(), Worksheets.end(), [&](Worksheet& worksheet) {
        return worksheet.Name == name;
    });
    return it == Worksheets.end() ? nullptr : &*it;
}

const Worksheet* ExcelData::GetWorksheet(std::string_view name) const
{
    auto it = std::find_if(Worksheets.begin(), Worksheets.end(), [&](const Worksheet& worksheet) {
        return worksheet.Name == name;
    });
    return it == Worksheets.end() ? nullptr : &*it;
}

ExcelData ExcelData::Load(std::span<const std::uint8_t> data)
{
    if (data.size() < 8 || SlLib::Extensions::readInt32(data, 0) != 0x54525353 /* SSRT */) {
        throw std::invalid_argument("Invalid excel data header!");
    }

    ExcelData excel;
    int32_t count = SlLib::Extensions::readInt32(data, 4);
    for (int32_t i = 0; i < count; ++i) {
        std::size_t entry = 0x8 + i * 4;
        std::size_t worksheetOffset = static_cast<std::size_t>(SlLib::Extensions::readInt32(data, entry));
        excel.Worksheets.emplace_back(Worksheet::Load(data, worksheetOffset));
    }

    return excel;
}

std::vector<std::uint8_t> ExcelData::Save() const
{
    // TODO: implement serialization once a target consumer is needed.
    return {};
}

} // namespace SlLib::Excel
