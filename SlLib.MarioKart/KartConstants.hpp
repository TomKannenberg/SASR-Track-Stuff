#pragma once

#include <string_view>
#include <unordered_map>

namespace SlLib::MarioKart {

struct KartConstants
{
    inline static constexpr std::string_view GameRoot = R"(F:\sart\game\pc\)";
    inline static constexpr std::string_view PublishRoot =
        R"(C:\Program Files (x86)\Steam\steamapps\common\Sonic & All-Stars Racing Transformed\Data\)";
    inline static constexpr std::string_view MarioRoot =
        R"(C:\Users\Aidan\AppData\Roaming\yuzu\dump\0100152000022000\romfs\)";
    inline static constexpr std::string_view ResourceCache = R"(F:\sart\mk8\cache\)";
    inline static constexpr std::string_view ShaderCache = R"(F:\sart\cache\)";
    inline static constexpr float GameScale = 0.1f;
};

extern const std::unordered_map<int, std::string_view> ObjectList;

} // namespace SlLib::MarioKart
