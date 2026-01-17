#include "SSRay.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/Math/Vector.hpp> // ensures operators

namespace SlLib::MarioKart::ssBVH {

SSRay SSRay::FromTwoPoints(SlLib::Math::Vector3 start, SlLib::Math::Vector3 end)
{
    auto delta = end - start;
    float length = SlLib::Math::length(delta);
    auto direction = length > 0.0f ? delta / length : SlLib::Math::Vector3{0.0f, 0.0f, 1.0f};

    return SSRay{start, direction};
}

} // namespace SlLib::MarioKart::ssBVH
