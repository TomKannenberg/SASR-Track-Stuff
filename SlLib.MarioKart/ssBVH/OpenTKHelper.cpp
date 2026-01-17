#include "OpenTKHelper.hpp"

#include <SlLib/Math/Vector.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace SlLib::MarioKart::ssBVH {

namespace {

inline SlLib::Math::Vector3 Normalize(SlLib::Math::Vector3 value)
{
    float len = SlLib::Math::length(value);
    if (len == 0.0f)
        return value;
    return value / len;
}

inline float Dot(SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b)
{
    return SlLib::Math::dot(a, b);
}

inline SlLib::Math::Vector3 Cross(SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b)
{
    return SlLib::Math::cross(a, b);
}

} // namespace

SSRay OpenTKHelper::MouseToWorldRay(SlLib::Math::Matrix4x4 const&,
    SlLib::Math::Matrix4x4 const&, Viewport viewport, SlLib::Math::Vector2 mouse)
{
    SlLib::Math::Vector3 near{mouse.X / static_cast<float>(viewport.Width), mouse.Y / static_cast<float>(viewport.Height), 0.0f};
    SlLib::Math::Vector3 far{near.X, near.Y, 1.0f};
    return SSRay::FromTwoPoints(near, far);
}

SlLib::Math::Vector3 OpenTKHelper::QuaternionToEuler(SlLib::Math::Quaternion const& q)
{
    float phi = std::atan2(q.Z * q.W + q.X * q.Y, 0.5f - q.Y * q.Y - q.Z * q.Z);
    float theta = std::asin(std::clamp(2.0f * (q.X * q.Z - q.Y * q.W), -1.0f, 1.0f));
    float gamma = std::atan2(q.Y * q.Z + q.X * q.W, 0.5f - q.Z * q.Z - q.W * q.W);
    return {gamma - std::numbers::pi_v<float>, theta, phi};
}

void OpenTKHelper::neededRotationAxisAngle(SlLib::Math::Vector3 dir1, SlLib::Math::Vector3 dir2,
    SlLib::Math::Vector3& axis, float& angle, bool normalizeInput)
{
    if (normalizeInput) {
        dir1 = Normalize(dir1);
        dir2 = Normalize(dir2);
    }

    auto cross = Cross(dir1, dir2);
    float crossLength = SlLib::Math::length(cross);
    if (crossLength <= std::numeric_limits<float>::epsilon())
    {
        axis = {1.0f, 0.0f, 0.0f};
        angle = std::numbers::pi_v<float>;
        return;
    }

    axis = cross / crossLength;
    float dot = Dot(dir1, dir2);
    angle = dot >= 1.0f ? 0.0f : std::acos(dot);
}

SlLib::Math::Quaternion OpenTKHelper::neededRotationQuat(SlLib::Math::Vector3 dir1,
    SlLib::Math::Vector3 dir2, bool normalize)
{
    SlLib::Math::Vector3 axis;
    float angle;
    neededRotationAxisAngle(dir1, dir2, axis, angle, normalize);
    return SlLib::Math::Quaternion{axis.X * std::sin(angle * 0.5f),
                                   axis.Y * std::sin(angle * 0.5f),
                                   axis.Z * std::sin(angle * 0.5f),
                                   std::cos(angle * 0.5f)};
}

SlLib::Math::Matrix4x4 OpenTKHelper::neededRotationMat(SlLib::Math::Vector3 dir1,
    SlLib::Math::Vector3 dir2, bool normalize)
{
    auto quat = neededRotationQuat(dir1, dir2, normalize);
    return SlLib::Math::CreateFromQuaternion(quat);
}

float OpenTKHelper::DistanceToLine(SSRay ray, SlLib::Math::Vector3 point, float& distanceAlongRay)
{
    auto a = ray.pos;
    auto n = ray.dir;
    auto p = point;
    auto diff = a - p;
    float t = Dot(diff, n);
    distanceAlongRay = -t;
    auto proj = diff - n * t;
    return SlLib::Math::length(proj);
}

std::vector<uint16_t> OpenTKHelper::GenerateLineIndices(std::vector<uint16_t> const& indices)
{
    std::vector<uint16_t> lineIndices;
    lineIndices.reserve(indices.size() * 2);
    for (size_t i = 2; i + 1 < indices.size(); i += 3)
    {
        auto v1 = indices[i - 2];
        auto v2 = indices[i - 1];
        auto v3 = indices[i];
        lineIndices.push_back(v1);
        lineIndices.push_back(v2);
        lineIndices.push_back(v1);
        lineIndices.push_back(v3);
        lineIndices.push_back(v2);
        lineIndices.push_back(v3);
    }
    return lineIndices;
}

std::mt19937& OpenTKHelper::DebugRandom()
{
    static std::mt19937 engine{std::random_device{}()};
    return engine;
}

} // namespace SlLib::MarioKart::ssBVH
