#pragma once

#include <cfloat>

#include "SlLib/Math/Vector.hpp"

namespace SlLib::Utilities {

constexpr float PI = 3.14159265358979323846f;
constexpr float Deg2Rad = PI / 180.0f;
constexpr float Rad2Deg = 180.0f / PI;

inline float Clamp(float value, float minimum, float maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

SlLib::Math::Quaternion LookRotation(SlLib::Math::Vector3 forward, SlLib::Math::Vector3 up);

SlLib::Math::Quaternion ToQuaternion(SlLib::Math::Vector3 rotation);

SlLib::Math::Quaternion FromEulerAngles(SlLib::Math::Vector3 rotation);

SlLib::Math::Vector3 ToEulerAngles(SlLib::Math::Quaternion const& value);

} // namespace SlLib::Utilities
