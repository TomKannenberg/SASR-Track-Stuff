#include "SurfaceFlags.hpp"

#include <array>
#include <utility>

namespace SlLib::Enums {

namespace {

inline constexpr auto surfaceFlagMappings()
{
    return std::array{
        std::pair{SurfaceFlags::Sticky, "Sticky"},
        std::pair{SurfaceFlags::WeakSticky, "WeakSticky"},
        std::pair{SurfaceFlags::DoubleGravity, "DoubleGravity"},
        std::pair{SurfaceFlags::SpaceWarp, "SpaceWarp"},
        std::pair{SurfaceFlags::CatchupDisabled, "CatchupDisabled"},
        std::pair{SurfaceFlags::StuntsDisabled, "StuntsDisabled"},
        std::pair{SurfaceFlags::BoostDisabled, "BoostDisabled"},
    };
}

} // namespace

std::string toString(SurfaceFlags flags)
{
    if (flags == SurfaceFlags::None) {
        return "None";
    }

    std::string output;
    for (auto [value, label] : surfaceFlagMappings()) {
        if ((flags & value) != SurfaceFlags::None) {
            if (!output.empty()) {
                output += '|';
            }
            output += label;
        }
    }

    return output.empty() ? "None" : output;
}

template <>
std::optional<SurfaceFlags> fromString<SurfaceFlags>(std::string_view text)
{
    for (auto [value, label] : surfaceFlagMappings()) {
        if (text == label) {
            return value;
        }
    }

    return std::nullopt;
}

} // namespace SlLib::Enums
