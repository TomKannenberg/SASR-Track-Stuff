#pragma once

#include "SSRay.hpp"
#include <SlLib/Math/Vector.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace SlLib::MarioKart::ssBVH {

struct SSAABB
{
    SlLib::Math::Vector3 Min{};
    SlLib::Math::Vector3 Max{};

    bool Contains(SlLib::Math::Vector3 point) const
    {
        return point.X >= Min.X && point.X <= Max.X &&
               point.Y >= Min.Y && point.Y <= Max.Y &&
               point.Z >= Min.Z && point.Z <= Max.Z;
    }

    bool Intersects(SSRay const& ray) const
    {
        float tmin = 0.0f;
        float tmax = std::numeric_limits<float>::max();

        auto checkAxis = [&](float min, float max, float origin, float direction) {
            if (std::abs(direction) < 1e-6f)
                return origin >= min && origin <= max;

            float t1 = (min - origin) / direction;
            float t2 = (max - origin) / direction;

            if (t1 > t2)
                std::swap(t1, t2);

            tmin = std::max(tmin, t1);
            tmax = std::min(tmax, t2);

            return tmax >= tmin;
        };

        return checkAxis(Min.X, Max.X, ray.pos.X, ray.dir.X) &&
               checkAxis(Min.Y, Max.Y, ray.pos.Y, ray.dir.Y) &&
               checkAxis(Min.Z, Max.Z, ray.pos.Z, ray.dir.Z);
    }
};

} // namespace SlLib::MarioKart::ssBVH
