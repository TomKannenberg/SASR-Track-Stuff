#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "SifParser.hpp"
#include "SlLib/SumoTool/Siff/LogicData.hpp"

namespace SeEditor {

struct LogicProbeInfo
{
    std::size_t BaseOffset = 0;
    int Version = 0;
    int Score = 0;
    int NumTriggers = 0;
    int NumLocators = 0;
};

bool LoadLogicFromSifChunks(std::vector<SifChunkInfo> const& chunks,
                            SlLib::SumoTool::Siff::LogicData& logic,
                            LogicProbeInfo& info,
                            std::string& error);

} // namespace SeEditor
