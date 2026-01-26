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

struct Options
{
    std::filesystem::path InputPath;
    std::filesystem::path OutputPath;
    std::filesystem::path GpuPath;
    int TreeIndex = -1;
    int AnimIndex = -1;
};

void PrintUsage()
{
    std::cout << "anim_debug <track.Forest|track.sif> <output.txt> --tree <index> --anim <index> [--gpu <track.gpu>]\n";
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

bool ParseArgs(int argc, char** argv, Options& out)
{
    if (argc < 5)
        return false;

    out.InputPath = argv[1];
    out.OutputPath = argv[2];
    for (int i = 3; i < argc; ++i)
    {
        std::string arg(argv[i]);
        if (arg == "--gpu" && i + 1 < argc)
        {
            out.GpuPath = argv[++i];
        }
        else if (arg == "--tree" && i + 1 < argc)
        {
            out.TreeIndex = std::stoi(argv[++i]);
        }
        else if (arg == "--anim" && i + 1 < argc)
        {
            out.AnimIndex = std::stoi(argv[++i]);
        }
        else
        {
            return false;
        }
    }
    return out.TreeIndex >= 0 && out.AnimIndex >= 0;
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

    std::vector<std::uint8_t> cpuData;
    std::vector<std::uint32_t> relocationOffsets;
    std::vector<std::uint8_t> gpuData;
    bool bigEndian = false;
    std::string error;

    bool parsed = false;
    try
    {
        std::vector<std::uint8_t> raw = ReadFile(options.InputPath);
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
        if (!ExtractForestFromSif(options.InputPath, cpuData, relocationOffsets, gpuData, bigEndian,
                                  options.GpuPath, error))
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

    if (library.Forests.empty() || !library.Forests[0].Forest)
    {
        std::cerr << "No forests loaded.\n";
        return 4;
    }

    auto const& forest = library.Forests[0].Forest;
    if (options.TreeIndex < 0 || options.TreeIndex >= static_cast<int>(forest->Trees.size()))
    {
        std::cerr << "Tree index out of range.\n";
        return 5;
    }

    auto const& tree = forest->Trees[static_cast<std::size_t>(options.TreeIndex)];
    if (!tree)
    {
        std::cerr << "Tree is null.\n";
        return 6;
    }
    if (options.AnimIndex < 0 || options.AnimIndex >= static_cast<int>(tree->AnimationEntries.size()))
    {
        std::cerr << "Animation index out of range.\n";
        return 7;
    }

    auto const& animEntry = tree->AnimationEntries[static_cast<std::size_t>(options.AnimIndex)];
    auto* anim = animEntry.Animation.get();
    if (!anim)
    {
        std::cerr << "Animation is null.\n";
        return 8;
    }

    bool ok = anim->DecodeType6Samples(*tree);
    std::ofstream out(options.OutputPath);
    if (!out)
    {
        std::cerr << "Failed to open " << options.OutputPath << " for writing.\n";
        return 9;
    }

    out << "TreeIndex=" << options.TreeIndex << "\n";
    out << "AnimIndex=" << options.AnimIndex << "\n";
    out << "AnimName=" << animEntry.AnimName << "\n";
    out << "AnimHash=" << animEntry.Hash << "\n";
    out << "Type=" << anim->Type << " Frames=" << anim->NumFrames << " Bones=" << anim->NumBones << "\n";
    out << "DecodeOk=" << (ok ? 1 : 0) << "\n";

    if (!ok)
        return 0;

    double maxAbsT = 0.0;
    double maxAbsS = 0.0;
    double maxAbsR = 0.0;
    double maxAbsW = 0.0;
    double minScale = std::numeric_limits<double>::infinity();
    double maxScale = 0.0;
    int nonFinite = 0;
    int hugeTrans = 0;

    int worstFrame = -1;
    int worstBone = -1;
    double worstValue = 0.0;

    for (int frame = 0; frame < anim->NumFrames; ++frame)
    {
        for (int bone = 0; bone < anim->NumBones; ++bone)
        {
            auto* sample = anim->GetSample(frame, bone);
            if (!sample)
                continue;
            auto check = [&](double v) {
                if (!std::isfinite(v))
                    ++nonFinite;
            };
            check(sample->Translation.X);
            check(sample->Translation.Y);
            check(sample->Translation.Z);
            check(sample->Rotation.X);
            check(sample->Rotation.Y);
            check(sample->Rotation.Z);
            check(sample->Rotation.W);
            check(sample->Scale.X);
            check(sample->Scale.Y);
            check(sample->Scale.Z);

            double tAbs = std::max({std::fabs(sample->Translation.X),
                                    std::fabs(sample->Translation.Y),
                                    std::fabs(sample->Translation.Z)});
            if (tAbs > maxAbsT)
                maxAbsT = tAbs;
            if (tAbs > 1.0e5)
                ++hugeTrans;

            maxAbsR = std::max(maxAbsR, std::fabs(static_cast<double>(sample->Rotation.X)));
            maxAbsR = std::max(maxAbsR, std::fabs(static_cast<double>(sample->Rotation.Y)));
            maxAbsR = std::max(maxAbsR, std::fabs(static_cast<double>(sample->Rotation.Z)));
            maxAbsW = std::max(maxAbsW, std::fabs(static_cast<double>(sample->Rotation.W)));

            double sMin = std::min({static_cast<double>(sample->Scale.X),
                                    static_cast<double>(sample->Scale.Y),
                                    static_cast<double>(sample->Scale.Z)});
            double sMax = std::max({static_cast<double>(sample->Scale.X),
                                    static_cast<double>(sample->Scale.Y),
                                    static_cast<double>(sample->Scale.Z)});
            minScale = std::min(minScale, sMin);
            maxScale = std::max(maxScale, sMax);
            double sAbs = std::max(std::fabs(static_cast<double>(sample->Scale.X)),
                                   std::max(std::fabs(static_cast<double>(sample->Scale.Y)),
                                            std::fabs(static_cast<double>(sample->Scale.Z))));
            maxAbsS = std::max(maxAbsS, sAbs);

            double worst = tAbs;
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Rotation.X)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Rotation.Y)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Rotation.Z)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Rotation.W)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Scale.X)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Scale.Y)));
            worst = std::max(worst, std::fabs(static_cast<double>(sample->Scale.Z)));
            if (worst > worstValue)
            {
                worstValue = worst;
                worstFrame = frame;
                worstBone = bone;
            }
        }
    }

    out << "MaxAbsTranslation=" << maxAbsT << "\n";
    out << "MaxAbsScale=" << maxAbsS << "\n";
    out << "MaxAbsRotationXYZ=" << maxAbsR << "\n";
    out << "MaxAbsRotationW=" << maxAbsW << "\n";
    out << "MinScale=" << minScale << " MaxScale=" << maxScale << "\n";
    out << "NonFiniteCount=" << nonFinite << "\n";
    out << "HugeTranslationCount=" << hugeTrans << "\n";
    out << "WorstFrame=" << worstFrame << " WorstBone=" << worstBone << " WorstValue=" << worstValue << "\n";

    if (worstFrame >= 0 && worstBone >= 0)
    {
        auto* sample = anim->GetSample(worstFrame, worstBone);
        if (sample)
        {
            out << "WorstSample T=(" << sample->Translation.X << "," << sample->Translation.Y << "," << sample->Translation.Z << ")\n";
            out << "WorstSample R=(" << sample->Rotation.X << "," << sample->Rotation.Y << "," << sample->Rotation.Z << "," << sample->Rotation.W << ")\n";
            out << "WorstSample S=(" << sample->Scale.X << "," << sample->Scale.Y << "," << sample->Scale.Z << ")\n";
        }
    }

    std::cout << "Wrote anim debug to " << options.OutputPath << "\n";
    return 0;
}
