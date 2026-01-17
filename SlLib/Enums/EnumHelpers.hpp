#pragma once

#include <optional>
#include <string_view>

namespace SlLib::Enums {

template <typename Enum>
std::optional<Enum> fromString(std::string_view text);

template <typename Enum, typename AllValuesFunc>
inline std::optional<Enum> fromStringFromAll(AllValuesFunc allValues, std::string_view text)
{
    for (auto candidate : allValues()) {
        if (text == toString(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
