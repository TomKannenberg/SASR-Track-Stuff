#include "SeEditor/Forest/ForestArchive.hpp"
#include "SifParser.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <zlib.h>

namespace {
using namespace SeEditor::Forest;
constexpr std::uint32_t kForestTag = 0x45524F46; // 'FORE'

std::uint32_t ReadUint32LE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

std::uint32_t ReadUint32BE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[3]) |
           (static_cast<std::uint32_t>(ptr[2]) << 8) |
           (static_cast<std::uint32_t>(ptr[1]) << 16) |
           (static_cast<std::uint32_t>(ptr[0]) << 24);
}

void StripLengthPrefixIfPresent(std::vector<std::uint8_t>& data)
{
    if (data.size() < 4)
        return;

    std::size_t size = data.size();
    std::uint32_t le = ReadUint32LE(data.data());
    std::uint32_t be = ReadUint32BE(data.data());
    if (le == size - 4 || le == size || be == size - 4 || be == size)
        data.erase(data.begin(), data.begin() + 4);
}

bool LooksLikeZlib(std::span<const std::uint8_t> data)
{
    if (data.size() < 2)
        return false;
    std::uint8_t cmf = data[0];
    std::uint8_t flg = data[1];
    if ((cmf & 0x0F) != 8)
        return false;
    return (((static_cast<int>(cmf) << 8) | flg) % 31) == 0;
}

std::vector<std::uint8_t> DecompressZlib(std::span<const std::uint8_t> stream)
{
    z_stream inflater{};
    inflater.next_in = const_cast<std::uint8_t*>(stream.data());
    inflater.avail_in = static_cast<decltype(inflater.avail_in)>(stream.size());

    constexpr int kZlibOkValue = Z_OK;
    constexpr int kZlibStreamEndValue = Z_STREAM_END;
    constexpr int kZlibFlushNone = 0;

    if (inflateInit(&inflater) != kZlibOkValue)
        throw std::runtime_error("Failed to init zlib inflater.");

    std::vector<std::uint8_t> result;
    std::vector<std::uint8_t> buffer(1 << 14);

    int status = kZlibOkValue;
    while (status != kZlibStreamEndValue)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<decltype(inflater.avail_out)>(buffer.size());

        status = inflate(&inflater, kZlibFlushNone);
        if (status != kZlibOkValue && status != kZlibStreamEndValue)
        {
            inflateEnd(&inflater);
            throw std::runtime_error("Zlib decompression failure.");
        }

        size_t have = buffer.size() - inflater.avail_out;
        result.insert(result.end(), buffer.begin(), buffer.begin() + have);
    }

    inflateEnd(&inflater);
    return result;
}

std::vector<std::uint8_t> MaybeDecompress(std::vector<std::uint8_t>&& raw, std::string const& label)
{
    std::span<const std::uint8_t> rawSpan(raw.data(), raw.size());
    if (!LooksLikeZlib(rawSpan))
    {
        StripLengthPrefixIfPresent(raw);
        return raw;
    }

    auto inflated = DecompressZlib(rawSpan);
    if (inflated.size() < sizeof(std::uint32_t))
        throw std::runtime_error("Decompressed " + label + " is too small.");

    std::uint32_t expected = ReadUint32LE(inflated.data());
    std::vector<std::uint8_t> payload(inflated.begin() + 4, inflated.end());
    if (expected != payload.size())
    {
        std::cerr << "[ForestUnpacker] Warning: " << label << " expected " << expected
                  << " bytes, got " << payload.size() << '.' << std::endl;
    }
    return payload;
}

std::vector<std::uint8_t> ReadFile(std::filesystem::path const& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Failed to open " + path.string());

    std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(input)), {});
    return data;
}

void PrintUsage()
{
    std::cout << "forest_unpacker <track.sif> [track.gpu] <output.Forest>\n"
              << "  Extracts the first Forest chunk and writes it with GPU data.\n";
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3 && argc != 4)
    {
        PrintUsage();
        return 1;
    }

    std::filesystem::path cpuPath(argv[1]);
    std::filesystem::path gpuPath;
    std::filesystem::path outputPath;
    if (argc == 3)
    {
        outputPath = argv[2];
    }
    else
    {
        gpuPath = argv[2];
        outputPath = argv[3];
    }

    std::vector<std::uint8_t> cpuRaw;
    try
    {
        cpuRaw = ReadFile(cpuPath);
    }
    catch (std::runtime_error const& ex)
    {
        std::cerr << ex.what() << '\n';
        return 2;
    }

    if (cpuRaw.empty())
    {
        std::cerr << "Input file is empty.\n";
        return 3;
    }

    std::string error;
    auto parsed = SeEditor::ParseSifFile(std::span<const std::uint8_t>(cpuRaw.data(), cpuRaw.size()), error);
    if (!parsed)
    {
        std::cerr << "SIF parse failed: " << error << '\n';
        return 4;
    }

    auto it = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
        [](SeEditor::SifChunkInfo const& chunk) { return chunk.TypeValue == kForestTag; });

    if (it == parsed->Chunks.end())
    {
        std::cerr << "No Forest chunk found inside " << cpuPath << ".\n";
        return 5;
    }

    auto const& chunk = *it;

    std::vector<std::uint8_t> gpuData;
    if (!gpuPath.empty())
    {
        try
        {
            auto raw = ReadFile(gpuPath);
            if (!raw.empty())
                gpuData = MaybeDecompress(std::move(raw), gpuPath.string());
        }
        catch (std::runtime_error const& ex)
        {
            std::cerr << "Failed to read GPU file: " << ex.what() << '\n';
            return 6;
        }
    }

    if (chunk.Data.size() > std::numeric_limits<std::uint32_t>::max())
    {
        std::cerr << "Forest chunk too large to package.\n";
        return 7;
    }

    if (chunk.Relocations.size() > std::numeric_limits<std::uint32_t>::max())
    {
        std::cerr << "Too many relocations.\n";
        return 8;
    }

    if (gpuData.size() > std::numeric_limits<std::uint32_t>::max())
    {
        std::cerr << "GPU payload too large.\n";
        return 9;
    }

    ForestArchiveHeader header{};
    if (chunk.BigEndian)
        header.Flags |= kForestArchiveFlagBigEndian;
    header.ChunkSize = static_cast<std::uint32_t>(chunk.Data.size());
    header.RelocationCount = static_cast<std::uint32_t>(chunk.Relocations.size());
    header.GpuSize = static_cast<std::uint32_t>(gpuData.size());

    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        std::cerr << "Failed to open " << outputPath << " for writing.\n";
        return 10;
    }

    output.write(reinterpret_cast<char const*>(&header), sizeof(header));
    if (!chunk.Data.empty())
        output.write(reinterpret_cast<char const*>(chunk.Data.data()), static_cast<std::streamsize>(chunk.Data.size()));

    for (std::size_t i = 0; i < chunk.Relocations.size(); ++i)
    {
        std::uint32_t value = chunk.Relocations[i];
        output.write(reinterpret_cast<char const*>(&value), sizeof(value));
    }

    if (!gpuData.empty())
        output.write(reinterpret_cast<char const*>(gpuData.data()), static_cast<std::streamsize>(gpuData.size()));

    std::cout << "Packed forest chunk (" << chunk.Data.size() << " bytes) and "
              << gpuData.size() << " bytes of GPU data into " << outputPath << ".\n";
    return 0;
}
