#include "LogicLoader.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

#include <array>

namespace SeEditor {

namespace {

constexpr std::uint32_t MakeTypeCode(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

std::uint32_t ReadUint32LE(std::vector<std::uint8_t> const& data, std::size_t offset)
{
    if (offset + 4 > data.size())
        return 0;
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

} // namespace

bool LoadLogicFromSifChunks(std::vector<SifChunkInfo> const& chunks,
                            SlLib::SumoTool::Siff::LogicData& logic,
                            LogicProbeInfo& info,
                            std::string& error)
{
    auto const* logicChunk = static_cast<SifChunkInfo const*>(nullptr);
    for (auto const& chunk : chunks)
    {
        if (chunk.TypeValue == MakeTypeCode('L', 'O', 'G', 'C'))
        {
            logicChunk = &chunk;
            break;
        }
    }

    if (!logicChunk)
    {
        error = "Logic chunk not found.";
        return false;
    }

    if (logicChunk->Data.empty())
    {
        error = "Logic chunk has no data.";
        return false;
    }

    if (logicChunk->Data.size() < 0x20)
    {
        error = "Logic chunk too small.";
        return false;
    }

    info.BaseOffset = 0;
    info.Version = static_cast<int>(ReadUint32LE(logicChunk->Data, 0x4));
    info.Score = 0;

    SlLib::Resources::Database::SlPlatform platform("pc", false, false, 0);
    SlLib::Serialization::ResourceLoadContext context(logicChunk->Data, {});
    context.Platform = &platform;
    context.Version = 0;
    context.Base = 0;
    context.GpuBase = 0;
    context.Position = 0;

    logic.Load(context);

    info.NumTriggers = static_cast<int>(logic.Triggers.size());
    info.NumLocators = static_cast<int>(logic.Locators.size());

    return true;
}

} // namespace SeEditor
