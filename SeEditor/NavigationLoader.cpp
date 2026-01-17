#include "NavigationLoader.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

#include <algorithm>
#include <cstring>

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

bool LoadNavigationFromSifChunks(std::vector<SifChunkInfo> const& chunks,
                                 SlLib::SumoTool::Siff::Navigation& navigation,
                                 NavigationProbeInfo& info,
                                 std::string& error)
{
    auto const* navChunk = static_cast<SifChunkInfo const*>(nullptr);
    for (auto const& chunk : chunks)
    {
        if (chunk.TypeValue == MakeTypeCode('T', 'R', 'A', 'K'))
        {
            navChunk = &chunk;
            break;
        }
    }

    if (!navChunk)
    {
        error = "Navigation chunk not found.";
        return false;
    }

    if (navChunk->Data.empty())
    {
        error = "Navigation chunk has no data.";
        return false;
    }

    info = NavigationProbeInfo{};
    info.HeaderSize = std::min<std::size_t>(info.Header.size(), navChunk->Data.size());
    std::memcpy(info.Header.data(), navChunk->Data.data(), info.HeaderSize);

    auto isPlausibleVersion = [](int version) {
        return version >= 0 && version <= 0x20;
    };
    auto isPlausibleCount = [](int value) {
        return value >= 0 && value < 1000000;
    };

    struct ProbeResult
    {
        bool Valid = false;
        int Version = -1;
        int Score = 0;
        std::size_t Base = 0;
    };

    auto probeBase = [&](std::size_t base) {
        ProbeResult result{};
        result.Base = base;
        if (base + 0x40 > navChunk->Data.size())
            return result;

        int version = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x4));
        if (!isPlausibleVersion(version))
            return result;

        int numVerts = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x0c));
        int numNormals = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x10));
        int numWaypoints = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x18));
        int numRacingLines = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x1c));
        int numTrackMarkers = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x20));
        int numSpatialGroups = static_cast<int>(ReadUint32LE(navChunk->Data, base + 0x30));

        int score = 0;
        if (isPlausibleCount(numVerts)) ++score;
        if (isPlausibleCount(numNormals)) ++score;
        if (isPlausibleCount(numWaypoints)) ++score;
        if (isPlausibleCount(numRacingLines)) ++score;
        if (isPlausibleCount(numTrackMarkers)) ++score;
        if (isPlausibleCount(numSpatialGroups)) ++score;

        result.Valid = score >= 4;
        result.Version = version;
        result.Score = score;
        return result;
    };

    std::array<std::size_t, 3> bases{0x0, 0x0c, 0x10};
    ProbeResult best{};
    for (auto base : bases)
    {
        auto probe = probeBase(base);
        if (!probe.Valid)
            continue;
        if (!best.Valid || probe.Score > best.Score)
            best = probe;
    }

    if (!best.Valid)
    {
        error = "Navigation header probe failed.";
        return false;
    }

    info.BaseOffset = best.Base;
    info.Version = best.Version;
    info.Score = best.Score;

    SlLib::Resources::Database::SlPlatform platform("pc", false, false, 0);
    SlLib::Serialization::ResourceLoadContext context(navChunk->Data, {});
    context.Platform = &platform;
    context.Version = 0;
    context.Base = static_cast<int>(best.Base);
    context.GpuBase = 0;
    context.Position = best.Base;

    navigation.Load(context);

    info.NumWaypoints = static_cast<int>(navigation.Waypoints.size());
    info.NumRacingLines = static_cast<int>(navigation.RacingLines.size());
    info.NumTrackMarkers = static_cast<int>(navigation.TrackMarkers.size());
    info.NumSpatialGroups = static_cast<int>(navigation.SpatialGroups.size());
    info.NumStarts = static_cast<int>(navigation.NavStarts.size());

    return true;
}

} // namespace SeEditor
