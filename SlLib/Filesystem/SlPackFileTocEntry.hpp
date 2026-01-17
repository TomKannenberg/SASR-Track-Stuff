#pragma once

#include <cstdint>
#include <string>

namespace SlLib::Filesystem {

struct SlPackFileTocEntry
{
    std::int32_t Hash = 0;
    bool IsDirectory = false;
    std::string Name;
    std::uint32_t Offset = 0;
    int Parent = -1;
    std::string Path;
    std::int32_t Size = 0;
};

} // namespace SlLib::Filesystem
