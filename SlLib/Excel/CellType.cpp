#include "CellType.hpp"

#include "SlLib/Utilities/SlUtil.hpp"

namespace SlLib::Excel {

namespace {

inline std::int32_t hash(const char* value)
{
    return SlLib::Utilities::sumoHash(value);
}

} // namespace

std::int32_t CellType::Float()
{
    static const std::int32_t value = hash("float");
    return value;
}

std::int32_t CellType::String()
{
    static const std::int32_t value = hash("string");
    return value;
}

std::int32_t CellType::Int()
{
    static const std::int32_t value = hash("int");
    return value;
}

std::int32_t CellType::Uint()
{
    static const std::int32_t value = hash("uint");
    return value;
}

} // namespace SlLib::Excel
