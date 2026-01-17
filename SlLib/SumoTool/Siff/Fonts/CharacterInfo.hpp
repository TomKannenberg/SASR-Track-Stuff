#pragma once

#include <cstdint>

namespace SlLib::SumoTool::Siff::Fonts {

class CharacterInfo
{
public:
    CharacterInfo();
    ~CharacterInfo();

    uint32_t Codepoint = 0;
};

} // namespace SlLib::SumoTool::Siff::Fonts
