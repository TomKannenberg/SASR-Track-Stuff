#pragma once

#include <cstdint>

namespace SeEditor::Forest {

constexpr std::uint32_t kForestArchiveMagic = 0x54525346; // 'FRST'
constexpr std::uint32_t kForestArchiveVersion = 2;
constexpr std::uint32_t kForestArchiveFlagBigEndian = 1u << 0;

struct ForestArchiveHeader
{
    std::uint32_t Magic = kForestArchiveMagic;
    std::uint32_t Version = kForestArchiveVersion;
    std::uint32_t Flags = 0;
    std::uint32_t ChunkSize = 0;
    std::uint32_t RelocationCount = 0;
    std::uint32_t GpuSize = 0;
};

} // namespace SeEditor::Forest
