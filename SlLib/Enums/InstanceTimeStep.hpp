#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class InstanceTimeStep
{
    Local,
    Global,
    Parent,
    ParentPlusLocal,
    GlobalPlusLocal,
    GlobalPlusParentPlusLocal,
};

inline constexpr auto allInstanceTimeSteps() noexcept -> std::array<InstanceTimeStep, 6>
{
    return {InstanceTimeStep::Local,
            InstanceTimeStep::Global,
            InstanceTimeStep::Parent,
            InstanceTimeStep::ParentPlusLocal,
            InstanceTimeStep::GlobalPlusLocal,
            InstanceTimeStep::GlobalPlusParentPlusLocal};
}

std::string toString(InstanceTimeStep step);

template <>
std::optional<InstanceTimeStep> fromString<InstanceTimeStep>(std::string_view text);

inline std::optional<InstanceTimeStep> fromStringInstanceTimeStep(std::string_view text)
{
    return fromString<InstanceTimeStep>(text);
}

} // namespace SlLib::Enums
