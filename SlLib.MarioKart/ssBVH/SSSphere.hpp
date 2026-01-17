#pragma once

#include <SlLib/Math/Vector.hpp>

namespace SlLib::MarioKart::ssBVH {

struct SSSphere
{
    SlLib::Math::Vector3 Center{};
    float Radius = 0.0f;

    bool Contains(SlLib::Math::Vector3 point) const
    {
        auto delta = point - Center;
        return SlLib::Math::length(delta) <= Radius;
    }
};

} // namespace SlLib::MarioKart::ssBVH
