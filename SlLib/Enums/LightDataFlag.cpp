#include "LightDataFlag.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto lightDataFlagLabels()
{
    return std::array{
        std::pair{LightDataFlag::BoxLight, "BoxLight"},
        std::pair{LightDataFlag::ShadowLight, "ShadowLight"},
        std::pair{LightDataFlag::LightGroup0, "LightGroup0"},
        std::pair{LightDataFlag::LightGroup1, "LightGroup1"},
        std::pair{LightDataFlag::LightGroup2, "LightGroup2"},
        std::pair{LightDataFlag::LightGroup3, "LightGroup3"},
        std::pair{LightDataFlag::LightGroup4, "LightGroup4"},
        std::pair{LightDataFlag::LightGroup5, "LightGroup5"},
        std::pair{LightDataFlag::LightUser0, "LightUser0"},
        std::pair{LightDataFlag::LightUser1, "LightUser1"},
        std::pair{LightDataFlag::LightOverlays, "LightOverlays"},
    };
}

} // namespace

std::string toString(LightDataFlag flags)
{
    if (flags == LightDataFlag::None) {
        return "None";
    }

    std::string result;
    for (auto [value, label] : lightDataFlagLabels()) {
        if ((flags & value) != LightDataFlag::None) {
            if (!result.empty()) {
                result += '|';
            }
            result += label;
        }
    }

    return result.empty() ? "None" : result;
}

template <>
std::optional<LightDataFlag> fromString<LightDataFlag>(std::string_view text)
{
    for (auto [value, label] : lightDataFlagLabels()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
