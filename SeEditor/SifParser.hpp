#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace SeEditor {

struct SifChunkInfo
{
    std::uint32_t TypeValue = 0;
    std::string Name;
    std::uint32_t DataSize = 0;
    std::uint32_t ChunkSize = 0;
    std::uint32_t RelocDataSize = 0;
    std::uint32_t RelocChunkSize = 0;
    std::vector<std::uint32_t> Relocations;
    bool BigEndian = false;
    std::vector<std::uint8_t> Data;
    std::vector<std::uint8_t> RawChunk;
    std::vector<std::uint8_t> RelocRaw;
};

struct SifParseResult
{
    bool WasCompressed = false;
    std::size_t DecompressedSize = 0;
    std::vector<SifChunkInfo> Chunks;
};

std::optional<SifParseResult> ParseSifFile(std::span<const std::uint8_t> raw, std::string& error);

} // namespace SeEditor
