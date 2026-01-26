#include "Forest/ForestArchive.hpp"
#include "Forest/ForestTypes.hpp"
#include "SifParser.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Resources/Database/SlResourceRelocation.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

#include <algorithm>
#include <cctype>
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
using namespace SeEditor;
using namespace SeEditor::Forest;

constexpr std::uint32_t kForestTag = 0x45524F46; // 'FORE'

struct Options
{
    std::filesystem::path InputPath;
    std::filesystem::path OutputDir;
    std::filesystem::path GpuPath;
    bool WriteChunks = true;
    bool WriteForestArchive = true;
    bool WriteAnimDump = true;
};

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
        std::cerr << "[SifUnpacker] Warning: " << label << " expected " << expected
                  << " bytes, got " << payload.size() << ".\n";
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

std::string FourCC(std::uint32_t value)
{
    char s[5] = {};
    s[0] = static_cast<char>(value & 0xFFu);
    s[1] = static_cast<char>((value >> 8) & 0xFFu);
    s[2] = static_cast<char>((value >> 16) & 0xFFu);
    s[3] = static_cast<char>((value >> 24) & 0xFFu);
    for (int i = 0; i < 4; ++i)
    {
        if (!std::isprint(static_cast<unsigned char>(s[i])))
            s[i] = '_';
    }
    return std::string(s, 4);
}

std::string SanitizeName(std::string name)
{
    for (char& c : name)
    {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-')
            c = '_';
    }
    return name;
}

std::string FormatChunkLabel(std::size_t index, SifChunkInfo const& chunk)
{
    std::string fourcc = FourCC(chunk.TypeValue);
    std::string name = chunk.Name.empty() ? "Chunk" : chunk.Name;
    name = SanitizeName(name);
    return std::to_string(index) + "_" + fourcc + "_" + name;
}

bool ParseArgs(int argc, char** argv, Options& out)
{
    if (argc < 3)
        return false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--gpu")
        {
            if (i + 1 >= argc)
                return false;
            out.GpuPath = argv[++i];
        }
        else if (arg == "--no-chunks")
        {
            out.WriteChunks = false;
        }
        else if (arg == "--no-forest")
        {
            out.WriteForestArchive = false;
        }
        else if (arg == "--no-anim")
        {
            out.WriteAnimDump = false;
        }
        else if (out.InputPath.empty())
        {
            out.InputPath = argv[i];
        }
        else if (out.OutputDir.empty())
        {
            out.OutputDir = argv[i];
        }
        else
        {
            return false;
        }
    }

    return !out.InputPath.empty() && !out.OutputDir.empty();
}

void PrintUsage()
{
    std::cout << "sif_unpacker <track.sif> <output_dir> [--gpu <track.gpu>] "
                 "[--no-chunks] [--no-forest] [--no-anim]\n"
              << "  Lists chunks and optionally writes chunk data, Forest archive, and animation summary.\n";
}

bool DumpAnimations(SifChunkInfo const& chunk,
                    std::span<const std::uint8_t> gpuData,
                    std::filesystem::path const& outPath,
                    std::string& error)
{
    if (chunk.Data.empty())
    {
        error = "Forest chunk has no data.";
        return false;
    }

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(chunk.Relocations.size());
    for (auto offset : chunk.Relocations)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        std::span<const std::uint8_t>(chunk.Data.data(), chunk.Data.size()),
        gpuData,
        std::move(relocations));
    SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
    SlLib::Resources::Database::SlPlatform xbox360("x360", true, false, 0);
    context.Platform = chunk.BigEndian ? &xbox360 : &win32;

    auto library = std::make_shared<ForestLibrary>();
    try
    {
        library->Load(context);
    }
    catch (std::exception const& ex)
    {
        error = ex.what();
        return false;
    }

    std::ofstream out(outPath);
    if (!out)
    {
        error = "Failed to open animation dump output.";
        return false;
    }

    out << "Forests=" << library->Forests.size() << "\n";
    for (std::size_t fi = 0; fi < library->Forests.size(); ++fi)
    {
        auto const& entry = library->Forests[fi];
        out << "Forest[" << fi << "] name=" << entry.Name
            << " hash=" << entry.Hash << "\n";
        if (!entry.Forest)
            continue;
        out << "  Trees=" << entry.Forest->Trees.size() << "\n";
        for (std::size_t ti = 0; ti < entry.Forest->Trees.size(); ++ti)
        {
            auto const& tree = entry.Forest->Trees[ti];
            if (!tree)
                continue;
            out << "  Tree[" << ti << "] hash=" << tree->Hash
                << " branches=" << tree->Branches.size()
                << " anims=" << tree->AnimationEntries.size() << "\n";
            for (std::size_t ai = 0; ai < tree->AnimationEntries.size(); ++ai)
            {
                auto const& animEntry = tree->AnimationEntries[ai];
                auto const* anim = animEntry.Animation.get();
                out << "    Anim[" << ai << "] name=" << animEntry.AnimName
                    << " hash=" << animEntry.Hash;
                if (!anim)
                {
                    out << " (null)\n";
                    continue;
                }
                out << " type=" << anim->Type
                    << " frames=" << anim->NumFrames
                    << " bones=" << anim->NumBones
                    << " uvBones=" << anim->NumUvBones
                    << " floatStreams=" << anim->NumFloatStreams
                    << " type6ParamOff=" << anim->Type6ParamDataOffset
                    << " type6BlockBytes=" << anim->Type6Block.size()
                    << "\n";
            }
        }
    }

    return true;
}

bool WriteForestArchive(SifChunkInfo const& chunk,
                        std::span<const std::uint8_t> gpuData,
                        std::filesystem::path const& outPath,
                        std::string& error)
{
    if (chunk.Data.size() > std::numeric_limits<std::uint32_t>::max())
    {
        error = "Forest chunk too large to package.";
        return false;
    }
    if (chunk.Relocations.size() > std::numeric_limits<std::uint32_t>::max())
    {
        error = "Too many relocations.";
        return false;
    }
    if (gpuData.size() > std::numeric_limits<std::uint32_t>::max())
    {
        error = "GPU payload too large.";
        return false;
    }

    ForestArchiveHeader header{};
    if (chunk.BigEndian)
        header.Flags |= kForestArchiveFlagBigEndian;
    header.ChunkSize = static_cast<std::uint32_t>(chunk.Data.size());
    header.RelocationCount = static_cast<std::uint32_t>(chunk.Relocations.size());
    header.GpuSize = static_cast<std::uint32_t>(gpuData.size());

    std::ofstream output(outPath, std::ios::binary);
    if (!output)
    {
        error = "Failed to open forest archive output.";
        return false;
    }

    output.write(reinterpret_cast<char const*>(&header), sizeof(header));
    if (!chunk.Data.empty())
        output.write(reinterpret_cast<char const*>(chunk.Data.data()),
                     static_cast<std::streamsize>(chunk.Data.size()));

    for (std::size_t i = 0; i < chunk.Relocations.size(); ++i)
    {
        std::uint32_t value = chunk.Relocations[i];
        output.write(reinterpret_cast<char const*>(&value), sizeof(value));
    }

    if (!gpuData.empty())
        output.write(reinterpret_cast<char const*>(gpuData.data()),
                     static_cast<std::streamsize>(gpuData.size()));

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!ParseArgs(argc, argv, options))
    {
        PrintUsage();
        return 1;
    }

    std::vector<std::uint8_t> cpuRaw;
    try
    {
        cpuRaw = ReadFile(options.InputPath);
    }
    catch (std::runtime_error const& ex)
    {
        std::cerr << ex.what() << "\n";
        return 2;
    }

    if (cpuRaw.empty())
    {
        std::cerr << "Input file is empty.\n";
        return 3;
    }

    std::filesystem::create_directories(options.OutputDir);

    std::string error;
    auto parsed = ParseSifFile(std::span<const std::uint8_t>(cpuRaw.data(), cpuRaw.size()), error);
    if (!parsed)
    {
        std::cerr << "SIF parse failed: " << error << "\n";
        return 4;
    }

    std::vector<std::uint8_t> gpuData;
    if (!options.GpuPath.empty())
    {
        try
        {
            auto raw = ReadFile(options.GpuPath);
            if (!raw.empty())
                gpuData = MaybeDecompress(std::move(raw), options.GpuPath.string());
        }
        catch (std::runtime_error const& ex)
        {
            std::cerr << "Failed to read GPU file: " << ex.what() << "\n";
            return 5;
        }
    }

    std::filesystem::path chunkListPath = options.OutputDir / "chunks.txt";
    std::ofstream chunkList(chunkListPath);
    if (!chunkList)
    {
        std::cerr << "Failed to write " << chunkListPath << "\n";
        return 6;
    }

    chunkList << "Chunks=" << parsed->Chunks.size() << "\n";
    std::cout << "Chunks=" << parsed->Chunks.size() << "\n";
    for (std::size_t i = 0; i < parsed->Chunks.size(); ++i)
    {
        auto const& chunk = parsed->Chunks[i];
        std::string fourcc = FourCC(chunk.TypeValue);
        chunkList << i << " " << fourcc << " name=" << chunk.Name
                  << " data=" << chunk.DataSize
                  << " reloc=" << chunk.Relocations.size()
                  << " endian=" << (chunk.BigEndian ? "BE" : "LE") << "\n";
        std::cout << i << " " << fourcc << " name=" << chunk.Name
                  << " data=" << chunk.DataSize
                  << " reloc=" << chunk.Relocations.size()
                  << " endian=" << (chunk.BigEndian ? "BE" : "LE") << "\n";

        if (!options.WriteChunks)
            continue;

        std::string label = FormatChunkLabel(i, chunk);
        std::filesystem::path dataPath = options.OutputDir / (label + ".bin");
        std::ofstream dataOut(dataPath, std::ios::binary);
        if (!dataOut)
        {
            std::cerr << "Failed to write " << dataPath << "\n";
            return 7;
        }
        if (!chunk.Data.empty())
            dataOut.write(reinterpret_cast<char const*>(chunk.Data.data()),
                          static_cast<std::streamsize>(chunk.Data.size()));

        if (!chunk.Relocations.empty())
        {
            std::filesystem::path relocPath = options.OutputDir / (label + ".relocs.txt");
            std::ofstream relocOut(relocPath);
            if (!relocOut)
            {
                std::cerr << "Failed to write " << relocPath << "\n";
                return 8;
            }
            for (std::uint32_t offset : chunk.Relocations)
                relocOut << "0x" << std::hex << offset << std::dec << "\n";
        }

        if (!chunk.RelocRaw.empty())
        {
            std::filesystem::path relocBinPath = options.OutputDir / (label + ".relo.bin");
            std::ofstream relocBin(relocBinPath, std::ios::binary);
            if (!relocBin)
            {
                std::cerr << "Failed to write " << relocBinPath << "\n";
                return 9;
            }
            relocBin.write(reinterpret_cast<char const*>(chunk.RelocRaw.data()),
                           static_cast<std::streamsize>(chunk.RelocRaw.size()));
        }
    }

    auto forestIt = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
        [](SifChunkInfo const& chunk) { return chunk.TypeValue == kForestTag; });

    if (forestIt != parsed->Chunks.end())
    {
        std::size_t forestIndex = static_cast<std::size_t>(forestIt - parsed->Chunks.begin());
        std::string forestLabel = FormatChunkLabel(forestIndex, *forestIt);
        std::filesystem::path forestDir = options.OutputDir / forestLabel;
        std::filesystem::create_directories(forestDir);
        std::span<const std::uint8_t> gpuSpan = gpuData.empty()
            ? std::span<const std::uint8_t>{}
            : std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

        if (options.WriteForestArchive)
        {
            std::filesystem::path outForest = forestDir / "forest.frst";
            std::string forestError;
            if (!WriteForestArchive(*forestIt, gpuSpan, outForest, forestError))
            {
                std::cerr << "Forest archive error: " << forestError << "\n";
                return 10;
            }
            std::cout << "Wrote forest archive to " << outForest << "\n";
        }

        if (options.WriteAnimDump)
        {
            std::filesystem::path animPath = forestDir / "animations.txt";
            std::string animError;
            if (!DumpAnimations(*forestIt, gpuSpan, animPath, animError))
            {
                std::cerr << "Animation dump error: " << animError << "\n";
                return 11;
            }
            std::cout << "Wrote animation dump to " << animPath << "\n";
        }
    }
    else
    {
        std::cout << "No Forest chunk found in input.\n";
    }

    return 0;
}
