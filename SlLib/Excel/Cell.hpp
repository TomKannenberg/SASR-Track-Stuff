#pragma once

#include <cstdint>
#include <string>
#include <variant>

namespace SlLib::Excel {

class CellType;

using CellValue = std::variant<int32_t, uint32_t, float, std::string>;

class Cell
{
public:
    Cell(int32_t name, int32_t type, CellValue value);

    bool IsInteger() const;
    bool IsUnsignedInteger() const;
    bool IsString() const;
    bool IsFloat() const;

    int32_t Name;
    int32_t Type;
    CellValue Value;
};

} // namespace SlLib::Excel
