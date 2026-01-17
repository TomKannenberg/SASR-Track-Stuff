#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "SifParser.hpp"
#include "SlLib/SumoTool/Siff/Navigation.hpp"

namespace SeEditor {

struct NavigationProbeInfo
{
    std::size_t BaseOffset = 0;
    int Version = 0;
    int Score = 0;
    int NumWaypoints = 0;
    int NumRacingLines = 0;
    int NumTrackMarkers = 0;
    int NumSpatialGroups = 0;
    int NumStarts = 0;
    std::array<std::uint8_t, 0x40> Header{};
    std::size_t HeaderSize = 0;
};

bool LoadNavigationFromSifChunks(std::vector<SifChunkInfo> const& chunks,
                                 SlLib::SumoTool::Siff::Navigation& navigation,
                                 NavigationProbeInfo& info,
                                 std::string& error);

} // namespace SeEditor
