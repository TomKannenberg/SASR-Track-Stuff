#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SlLib::MarioKart::ssBVH {

struct SSRay
{
    SlLib::Math::Vector3 pos{};
    SlLib::Math::Vector3 dir{0.0f, 0.0f, 1.0f};

    static SSRay FromTwoPoints(SlLib::Math::Vector3 start, SlLib::Math::Vector3 end);

    SlLib::Math::Vector3 PointAt(float distance) const
    {
        return pos + dir * distance;
    }
};

} // namespace SlLib::MarioKart::ssBVH
