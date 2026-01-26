#include "SeEditor/Forest/ForestArchive.hpp"
#include "Forest/ForestTypes.hpp"
#include "SifParser.hpp"

#include <SlLib/Resources/Database/SlPlatform.hpp>
#include <SlLib/Resources/Database/SlResourceRelocation.hpp>
#include <SlLib/Serialization/ResourceLoadContext.hpp>

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
#include <string>
#include <vector>

namespace {
using namespace SeEditor;
using namespace SeEditor::Forest;

constexpr std::uint32_t kForestTag = 0x45524F46; // 'FORE'

void PrintUsage()
{
    std::cout << "forest_tree_dump <track.Forest|track.sif> <output_dir> [--gpu <track.gpu>]\n"
              << "  Writes per-tree folders with hierarchy and animation lists.\n";
}

std::vector<std::uint8_t> ReadFile(std::filesystem::path const& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Failed to open " + path.string());
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)), {});
}

bool TryParseForestArchive(std::span<const std::uint8_t> input,
                           std::vector<std::uint8_t>& cpuData,
                           std::vector<std::uint32_t>& relocations,
                           std::vector<std::uint8_t>& gpuData,
                           bool& bigEndian)
{
    if (input.size() < 20)
        return false;

    auto readU32 = [&](std::size_t offset) -> std::uint32_t {
        if (offset + 4 > input.size())
            return 0;
        return static_cast<std::uint32_t>(input[offset]) |
               (static_cast<std::uint32_t>(input[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(input[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(input[offset + 3]) << 24);
    };

    std::uint32_t magic = readU32(0);
    std::uint32_t version = readU32(4);
    if (magic != kForestArchiveMagic)
        return false;

    std::size_t headerSize = 0;
    std::uint32_t flags = 0;
    std::size_t chunkSize = 0;
    std::size_t relocCount = 0;
    std::size_t gpuSize = 0;

    if (version == 1)
    {
        headerSize = 16;
        flags = readU32(8);
        chunkSize = readU32(12);
    }
    else
    {
        headerSize = 24;
        flags = readU32(8);
        chunkSize = readU32(12);
        relocCount = readU32(16);
        gpuSize = readU32(20);
    }

    if (headerSize == 0 || input.size() < headerSize)
        return false;

    bigEndian = (flags & kForestArchiveFlagBigEndian) != 0;
    std::size_t cpuStart = headerSize;
    std::size_t cpuEnd = cpuStart + chunkSize;
    if (cpuEnd > input.size())
        return false;

    cpuData.assign(input.begin() + static_cast<std::ptrdiff_t>(cpuStart),
                   input.begin() + static_cast<std::ptrdiff_t>(cpuEnd));

    std::size_t relocStart = cpuEnd;
    std::size_t relocBytes = relocCount * sizeof(std::uint32_t);
    if (relocStart + relocBytes > input.size())
        return false;
    relocations.clear();
    relocations.reserve(relocCount);
    for (std::size_t i = 0; i < relocCount; ++i)
    {
        std::size_t off = relocStart + i * 4;
        std::uint32_t value = readU32(off);
        relocations.push_back(value);
    }

    std::size_t gpuStart = relocStart + relocBytes;
    if (gpuSize > 0 && gpuStart + gpuSize <= input.size())
    {
        gpuData.assign(input.begin() + static_cast<std::ptrdiff_t>(gpuStart),
                       input.begin() + static_cast<std::ptrdiff_t>(gpuStart + gpuSize));
    }

    return true;
}

bool LoadForestLibrary(std::vector<std::uint8_t> const& cpuData,
                       std::vector<std::uint32_t> const& relocationOffsets,
                       std::vector<std::uint8_t> const& gpuData,
                       bool bigEndian,
                       ForestLibrary& outLibrary,
                       std::string& error)
{
    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(relocationOffsets.size());
    for (auto offset : relocationOffsets)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        cpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(cpuData.data(), cpuData.size()),
        gpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(gpuData.data(), gpuData.size()),
        std::move(relocations));
    SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
    SlLib::Resources::Database::SlPlatform xbox360("x360", true, false, 0);
    context.Platform = bigEndian ? &xbox360 : &win32;
    context.Version = 0;

    try
    {
        outLibrary.Load(context);
    }
    catch (std::exception const& ex)
    {
        error = ex.what();
        return false;
    }
    return true;
}

bool ExtractForestFromSif(std::filesystem::path const& path,
                          std::vector<std::uint8_t>& cpuData,
                          std::vector<std::uint32_t>& relocations,
                          std::vector<std::uint8_t>& gpuData,
                          bool& bigEndian,
                          std::filesystem::path const& gpuPath,
                          std::string& error)
{
    std::vector<std::uint8_t> raw = ReadFile(path);
    if (raw.empty())
    {
        error = "Input file is empty.";
        return false;
    }

    auto parsed = ParseSifFile(std::span<const std::uint8_t>(raw.data(), raw.size()), error);
    if (!parsed)
        return false;

    auto it = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
        [](SifChunkInfo const& chunk) { return chunk.TypeValue == kForestTag; });
    if (it == parsed->Chunks.end())
    {
        error = "No Forest chunk found.";
        return false;
    }

    cpuData = it->Data;
    relocations = it->Relocations;
    bigEndian = it->BigEndian;

    if (!gpuPath.empty())
    {
        try
        {
            gpuData = ReadFile(gpuPath);
        }
        catch (std::exception const& ex)
        {
            error = ex.what();
            return false;
        }
    }

    return true;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        PrintUsage();
        return 1;
    }

    std::filesystem::path inputPath;
    std::filesystem::path outputDir;
    std::filesystem::path gpuPath;

    inputPath = argv[1];
    outputDir = argv[2];
    for (int i = 3; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--gpu" && i + 1 < argc)
        {
            gpuPath = argv[++i];
        }
        else
        {
            PrintUsage();
            return 1;
        }
    }

    std::vector<std::uint8_t> cpuData;
    std::vector<std::uint32_t> relocationOffsets;
    std::vector<std::uint8_t> gpuData;
    bool bigEndian = false;
    std::string error;

    bool parsed = false;
    try
    {
        std::vector<std::uint8_t> raw = ReadFile(inputPath);
        if (TryParseForestArchive(std::span<const std::uint8_t>(raw.data(), raw.size()),
                                  cpuData, relocationOffsets, gpuData, bigEndian))
        {
            parsed = true;
        }
    }
    catch (std::exception const&)
    {
        parsed = false;
    }

    if (!parsed)
    {
        if (!ExtractForestFromSif(inputPath, cpuData, relocationOffsets, gpuData, bigEndian, gpuPath, error))
        {
            std::cerr << "Failed to parse input: " << error << "\n";
            return 2;
        }
    }

    ForestLibrary library;
    if (!LoadForestLibrary(cpuData, relocationOffsets, gpuData, bigEndian, library, error))
    {
        std::cerr << "Failed to load forest library: " << error << "\n";
        return 3;
    }

    std::filesystem::create_directories(outputDir);
    std::filesystem::path indexPath = outputDir / "index.txt";
    std::ofstream indexOut(indexPath);
    if (!indexOut)
    {
        std::cerr << "Failed to open " << indexPath << " for writing.\n";
        return 4;
    }

    indexOut << "Forests=" << library.Forests.size() << "\n";
    for (std::size_t fi = 0; fi < library.Forests.size(); ++fi)
    {
        auto const& entry = library.Forests[fi];
        indexOut << "Forest[" << fi << "] name=" << entry.Name << " hash=" << entry.Hash << "\n";
        if (!entry.Forest)
            continue;

        indexOut << "  Trees=" << entry.Forest->Trees.size() << "\n";
        for (std::size_t ti = 0; ti < entry.Forest->Trees.size(); ++ti)
        {
            auto const& tree = entry.Forest->Trees[ti];
            if (!tree)
                continue;

            std::string treeFolder = "tree_" + std::to_string(ti) + "_hash_" + std::to_string(tree->Hash);
            std::filesystem::path treeDir = outputDir / treeFolder;
            std::filesystem::create_directories(treeDir);

            std::filesystem::path metaPath = treeDir / "tree.txt";
            std::ofstream metaOut(metaPath);
            if (!metaOut)
            {
                std::cerr << "Failed to open " << metaPath << " for writing.\n";
                return 5;
            }
            metaOut << "ForestIndex=" << fi << "\n";
            metaOut << "TreeIndex=" << ti << "\n";
            metaOut << "TreeHash=" << tree->Hash << "\n";
            metaOut << "Branches=" << tree->Branches.size() << "\n";
            metaOut << "Animations=" << tree->AnimationEntries.size() << "\n";

            std::filesystem::path branchesPath = treeDir / "branches.txt";
            std::ofstream branchesOut(branchesPath);
            if (!branchesOut)
            {
                std::cerr << "Failed to open " << branchesPath << " for writing.\n";
                return 6;
            }
            for (std::size_t bi = 0; bi < tree->Branches.size(); ++bi)
            {
                auto const& branch = tree->Branches[bi];
                if (!branch)
                    continue;
                branchesOut << "Branch[" << bi << "] parent=" << branch->Parent
                            << " child=" << branch->Child
                            << " sibling=" << branch->Sibling
                            << " flags=" << branch->Flags
                            << " name=" << branch->Name << "\n";
            }

            std::filesystem::path animPath = treeDir / "animations.txt";
            std::ofstream animOut(animPath);
            if (!animOut)
            {
                std::cerr << "Failed to open " << animPath << " for writing.\n";
                return 7;
            }
            for (std::size_t ai = 0; ai < tree->AnimationEntries.size(); ++ai)
            {
                auto const& animEntry = tree->AnimationEntries[ai];
                auto const* anim = animEntry.Animation.get();
                animOut << "Anim[" << ai << "] name=" << animEntry.AnimName
                        << " hash=" << animEntry.Hash;
                if (!anim)
                {
                    animOut << " (null)\n";
                    continue;
                }
                animOut << " type=" << anim->Type
                        << " frames=" << anim->NumFrames
                        << " bones=" << anim->NumBones
                        << " uvBones=" << anim->NumUvBones
                        << " floatStreams=" << anim->NumFloatStreams
                        << " type6ParamOff=" << anim->Type6ParamDataOffset
                        << " type6BlockBytes=" << anim->Type6Block.size()
                        << "\n";
            }

            indexOut << "  Tree[" << ti << "] dir=" << treeFolder
                     << " branches=" << tree->Branches.size()
                     << " anims=" << tree->AnimationEntries.size() << "\n";
        }
    }

    std::cout << "Wrote tree dump to " << outputDir << "\n";
    return 0;
}
