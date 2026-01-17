#include "MathUtils.hpp"

#include <cmath>

namespace SlLib::Utilities {

SlLib::Math::Quaternion LookRotation(SlLib::Math::Vector3 forward, SlLib::Math::Vector3 up)
{
    forward = SlLib::Math::normalize(forward);
    SlLib::Math::Vector3 right = SlLib::Math::normalize(SlLib::Math::cross(up, forward));
    up = SlLib::Math::cross(forward, right);

    float m00 = right.X;
    float m01 = right.Y;
    float m02 = right.Z;
    float m10 = up.X;
    float m11 = up.Y;
    float m12 = up.Z;
    float m20 = forward.X;
    float m21 = forward.Y;
    float m22 = forward.Z;

    float num8 = (m00 + m11) + m22;
    SlLib::Math::Quaternion quaternion{};

    if (num8 > 0.0f) {
        float num = std::sqrt(num8 + 1.0f);
        quaternion.W = num * 0.5f;
        num = 0.5f / num;
        quaternion.X = (m12 - m21) * num;
        quaternion.Y = (m20 - m02) * num;
        quaternion.Z = (m01 - m10) * num;
        return quaternion;
    }

    if ((m00 >= m11) && (m00 >= m22)) {
        float num7 = std::sqrt(((1.0f + m00) - m11) - m22);
        float num4 = 0.5f / num7;
        quaternion.X = 0.5f * num7;
        quaternion.Y = (m01 + m10) * num4;
        quaternion.Z = (m02 + m20) * num4;
        quaternion.W = (m12 - m21) * num4;
        return quaternion;
    }

    if (m11 > m22) {
        float num6 = std::sqrt(((1.0f + m11) - m00) - m22);
        float num3 = 0.5f / num6;
        quaternion.X = (m10 + m01) * num3;
        quaternion.Y = 0.5f * num6;
        quaternion.Z = (m21 + m12) * num3;
        quaternion.W = (m20 - m02) * num3;
        return quaternion;
    }

    float num5 = std::sqrt(((1.0f + m22) - m00) - m11);
    float num2 = 0.5f / num5;
    quaternion.X = (m20 + m02) * num2;
    quaternion.Y = (m21 + m12) * num2;
    quaternion.Z = 0.5f * num5;
    quaternion.W = (m01 - m10) * num2;
    return quaternion;
}

SlLib::Math::Quaternion ToQuaternion(SlLib::Math::Vector3 rotation)
{
    float cy = std::cos(rotation.Z * 0.5f);
    float sy = std::sin(rotation.Z * 0.5f);
    float cp = std::cos(rotation.Y * 0.5f);
    float sp = std::sin(rotation.Y * 0.5f);
    float cr = std::cos(rotation.X * 0.5f);
    float sr = std::sin(rotation.X * 0.5f);

    return {sr * cp * cy - cr * sp * sy,
            cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
}

SlLib::Math::Quaternion FromEulerAngles(SlLib::Math::Vector3 rotation)
{
    rotation = rotation * Deg2Rad;
    SlLib::Math::Quaternion xRotation = SlLib::Math::CreateFromAxisAngle({1.0f, 0.0f, 0.0f}, rotation.X);
    SlLib::Math::Quaternion yRotation = SlLib::Math::CreateFromAxisAngle({0.0f, 1.0f, 0.0f}, rotation.Y);
    SlLib::Math::Quaternion zRotation = SlLib::Math::CreateFromAxisAngle({0.0f, 0.0f, 1.0f}, rotation.Z);

    SlLib::Math::Quaternion q = zRotation * yRotation * xRotation;
    if (q.W < 0.0f) {
        q = q * -1.0f;
    }

    return q;
}

SlLib::Math::Vector3 ToEulerAngles(SlLib::Math::Quaternion const& value)
{
    SlLib::Math::Matrix4x4 matrix = SlLib::Math::CreateFromQuaternion(value);
    float y = std::asin(Clamp(matrix(0, 2), -1.0f, 1.0f));
    float x;
    float z;

    if (std::abs(matrix(0, 2)) < 0.99999f) {
        x = std::atan2(-matrix(1, 2), matrix(2, 2));
        z = std::atan2(-matrix(0, 1), matrix(0, 0));
    } else {
        x = std::atan2(matrix(2, 1), matrix(1, 1));
        z = 0.0f;
    }

    return (SlLib::Math::Vector3{x, y, z} * -Rad2Deg);
}

} // namespace SlLib::Utilities
