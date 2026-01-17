#pragma once

#include <cstdint>
#include <string>

namespace SlLib::Filesystem {

struct SsrPackFileEntry
{
    int CompressedSize = 0;
    int FilenameHash = 0;
    int Flags = 0;
    std::uint32_t Offset = 0;
    int Size = 0;
    int TempHackEntryFileOffset = 0;
    std::string Path;
};

} // namespace SlLib::Filesystem
