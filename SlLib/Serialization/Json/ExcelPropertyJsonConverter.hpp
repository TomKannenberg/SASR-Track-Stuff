#pragma once

#include <string>

namespace SlLib::Serialization::Json {

class ExcelPropertyJsonConverter
{
public:
    static std::string Convert(std::string const& value);
};

} // namespace SlLib::Serialization::Json
