#include "InstanceTimeStep.hpp"

namespace SlLib::Enums {

std::string toString(InstanceTimeStep step)
{
    switch (step) {
    case InstanceTimeStep::Local:
        return "Local";
    case InstanceTimeStep::Global:
        return "Global";
    case InstanceTimeStep::Parent:
        return "Parent";
    case InstanceTimeStep::ParentPlusLocal:
        return "ParentPlusLocal";
    case InstanceTimeStep::GlobalPlusLocal:
        return "GlobalPlusLocal";
    case InstanceTimeStep::GlobalPlusParentPlusLocal:
        return "GlobalPlusParentPlusLocal";
    }

    return "Unknown";
}

template <>
std::optional<InstanceTimeStep> fromString<InstanceTimeStep>(std::string_view text)
{
    return fromStringFromAll<InstanceTimeStep>(allInstanceTimeSteps, text);
}

} // namespace SlLib::Enums
