#pragma once

#include "SSRay.hpp"
#include <SlLib/Math/Vector.hpp>

#include <cmath>
#include <random>
#include <vector>

namespace SlLib::MarioKart::ssBVH {

struct Viewport
{
    int Width = 1;
    int Height = 1;
};

class OpenTKHelper
{
public:
    static SSRay MouseToWorldRay(SlLib::Math::Matrix4x4 const& projection,
        SlLib::Math::Matrix4x4 const& view,
        Viewport viewport,
        SlLib::Math::Vector2 mouse);

    static SlLib::Math::Vector3 QuaternionToEuler(SlLib::Math::Quaternion const& q);
    static void neededRotationAxisAngle(SlLib::Math::Vector3 dir1, SlLib::Math::Vector3 dir2,
        SlLib::Math::Vector3& axis, float& angle, bool normalizeInput = true);

    static SlLib::Math::Quaternion neededRotationQuat(SlLib::Math::Vector3 dir1,
        SlLib::Math::Vector3 dir2, bool normalize = true);

    static SlLib::Math::Matrix4x4 neededRotationMat(SlLib::Math::Vector3 dir1,
        SlLib::Math::Vector3 dir2, bool normalize = true);

    static float DistanceToLine(SSRay ray, SlLib::Math::Vector3 point, float& distanceAlongRay);
    static std::vector<uint16_t> GenerateLineIndices(std::vector<uint16_t> const& indices);

    static std::mt19937& DebugRandom();
};

} // namespace SlLib::MarioKart::ssBVH
#include <SlLib/Math/Vector.hpp>
