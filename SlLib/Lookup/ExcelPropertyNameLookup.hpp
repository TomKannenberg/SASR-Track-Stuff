#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <cstdint>

namespace SlLib::Lookup {

class ExcelPropertyNameLookup
{
public:
    static std::optional<std::string> GetPropertyName(std::uint32_t hash);

private:
    static const std::unordered_map<std::uint32_t, std::string>& Lookup();
};

} // namespace SlLib::Lookup
