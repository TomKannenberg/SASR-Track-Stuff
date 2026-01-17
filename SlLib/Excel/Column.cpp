#include "Column.hpp"

#include <stdexcept>
#include <variant>

namespace SlLib::Excel {

const Cell* Column::GetParamByName(int32_t hash) const
{
    for (auto& cell : Cells) {
        if (cell.Name == hash) {
            return &cell;
        }
    }
    return nullptr;
}

const Cell* Column::GetParamByName(std::string_view name) const
{
    return GetParamByName(SlLib::Utilities::sumoHash(name));
}

float Column::GetFloat(std::string_view name, float fallback) const
{
    const Cell* cell = GetParamByName(name);
    if (cell == nullptr || cell->Type != CellType::Float()) {
        return fallback;
    }
    if (auto value = std::get_if<float>(&cell->Value)) {
        return *value;
    }
    return fallback;
}

int32_t Column::GetInt(std::string_view name, int32_t fallback) const
{
    const Cell* cell = GetParamByName(name);
    if (cell == nullptr || cell->Type != CellType::Int()) {
        return fallback;
    }
    if (auto value = std::get_if<int32_t>(&cell->Value)) {
        return *value;
    }
    return fallback;
}

uint32_t Column::GetUint(std::string_view name, uint32_t fallback) const
{
    const Cell* cell = GetParamByName(name);
    if (cell == nullptr || cell->Type != CellType::Uint()) {
        return fallback;
    }
    if (auto value = std::get_if<uint32_t>(&cell->Value)) {
        return *value;
    }
    return fallback;
}

std::string Column::GetString(std::string_view name, std::string fallback) const
{
    const Cell* cell = GetParamByName(name);
    if (cell == nullptr || cell->Type != CellType::String()) {
        return fallback;
    }
    if (auto value = std::get_if<std::string>(&cell->Value)) {
        return *value;
    }
    return fallback;
}

Column Column::Load(std::span<const std::uint8_t> data, std::size_t offset)
{
    Column column;
    column.Name = SlLib::Extensions::readString(data, offset + 8);

    int32_t count = SlLib::Extensions::readInt32(data, offset);
    for (int32_t i = 0; i < count; ++i) {
        std::size_t entry = offset + 0x48 + i * 0xc;

        int32_t name = SlLib::Extensions::readInt32(data, entry);
        int32_t type = SlLib::Extensions::readInt32(data, entry + 4);
        CellValue value;

        if (type == CellType::String()) {
            int32_t stringOffset = SlLib::Extensions::readInt32(data, entry + 8);
            value = SlLib::Extensions::readString(data, static_cast<std::size_t>(stringOffset));
        } else if (type == CellType::Int()) {
            value = SlLib::Extensions::readInt32(data, entry + 8);
        } else if (type == CellType::Uint()) {
            value = static_cast<uint32_t>(SlLib::Extensions::readInt32(data, entry + 8));
        } else if (type == CellType::Float()) {
            value = SlLib::Extensions::readFloat(data, entry + 8);
        } else {
            throw std::runtime_error("Invalid data type found in column data!");
        }

        column.Cells.emplace_back(name, type, std::move(value));
    }

    return column;
}

} // namespace SlLib::Excel
