#include "Cell.hpp"

#include "CellType.hpp"

namespace SlLib::Excel {

Cell::Cell(int32_t name, int32_t type, CellValue value)
    : Name(name)
    , Type(type)
    , Value(std::move(value))
{
}

bool Cell::IsInteger() const
{
    return Type == CellType::Int();
}

bool Cell::IsUnsignedInteger() const
{
    return Type == CellType::Uint();
}

bool Cell::IsString() const
{
    return Type == CellType::String();
}

bool Cell::IsFloat() const
{
    return Type == CellType::Float();
}

} // namespace SlLib::Excel
