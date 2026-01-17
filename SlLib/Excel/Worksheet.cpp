#include "Worksheet.hpp"

#include <algorithm>

namespace SlLib::Excel {

Column* Worksheet::GetColumnByName(std::string_view name)
{
    auto it = std::find_if(Columns.begin(), Columns.end(), [&](const Column& column) {
        return column.Name == name;
    });
    return it == Columns.end() ? nullptr : &*it;
}

const Column* Worksheet::GetColumnByName(std::string_view name) const
{
    auto it = std::find_if(Columns.begin(), Columns.end(), [&](const Column& column) {
        return column.Name == name;
    });
    return it == Columns.end() ? nullptr : &*it;
}

Worksheet Worksheet::Load(std::span<const std::uint8_t> data, std::size_t offset)
{
    Worksheet worksheet;
    worksheet.Name = SlLib::Extensions::readString(data, offset + 4);

    int32_t count = SlLib::Extensions::readInt32(data, offset + 0x44);
    for (int32_t i = 0; i < count; ++i) {
        std::size_t entry = offset + 0x48 + i * 4;
        std::size_t columnOffset =
            static_cast<std::size_t>(SlLib::Extensions::readInt32(data, entry));
        worksheet.Columns.emplace_back(Column::Load(data, columnOffset));
    }

    return worksheet;
}

} // namespace SlLib::Excel
