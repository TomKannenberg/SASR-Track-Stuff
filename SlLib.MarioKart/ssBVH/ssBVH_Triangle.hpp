#pragma once

#include "SSAABB.hpp"
#include <SlLib/Math/Vector.hpp>

#include <algorithm>
namespace SlLib::MarioKart::ssBVH {

struct ssBVH_Triangle
{
    SlLib::Math::Vector3 A{};
    SlLib::Math::Vector3 B{};
    SlLib::Math::Vector3 C{};
    int Id = -1;

    SSAABB Bounds() const
    {
        SSAABB box;
        box.Min.X = std::min({A.X, B.X, C.X});
        box.Min.Y = std::min({A.Y, B.Y, C.Y});
        box.Min.Z = std::min({A.Z, B.Z, C.Z});
        box.Max.X = std::max({A.X, B.X, C.X});
        box.Max.Y = std::max({A.Y, B.Y, C.Y});
        box.Max.Z = std::max({A.Z, B.Z, C.Z});
        return box;
    }
};

} // namespace SlLib::MarioKart::ssBVH
