#pragma once

#include <vector>

#include "../../../Math/Vector.hpp"
#include "NavRacingLineSeg.hpp"

namespace SlLib::SumoTool::Siff::NavData {

class NavRacingLine
{
public:
    NavRacingLine();
    ~NavRacingLine();

    std::vector<NavRacingLineSeg> Segments;
};

} // namespace SlLib::SumoTool::Siff::NavData
