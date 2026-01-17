#include "SifParser.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

namespace {
constexpr std::uint32_t kForestTag = 0x45524F46; // 'FORE'

void PrintUsage()
{
    std::cout << "forest_extractor <track.sif> <output.Forest>\n"
              << "  Extracts the first Forest chunk from the provided SIF payload.\n";
}
} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        PrintUsage();
        return 1;
    }

    std::filesystem::path inputPath(argv[1]);
    std::filesystem::path outputPath(argv[2]);

    std::ifstream file(inputPath, std::ios::binary);
    if (!file)
    {
        std::cerr << "Failed to open " << inputPath << " for reading.\n";
        return 2;
    }

    std::vector<char> rawBuffer((std::istreambuf_iterator<char>(file)), {});
    if (rawBuffer.empty())
    {
        std::cerr << "Input file is empty.\n";
        return 3;
    }

    std::vector<std::uint8_t> data(rawBuffer.begin(), rawBuffer.end());
    std::string error;
    auto parsed = SeEditor::ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
    if (!parsed)
    {
        std::cerr << "SIF parse failed: " << error << '\n';
        return 4;
    }

    auto it = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
        [](SeEditor::SifChunkInfo const& chunk) { return chunk.TypeValue == kForestTag; });

    if (it == parsed->Chunks.end())
    {
        std::cerr << "No Forest chunk found inside " << inputPath << ".\n";
        return 5;
    }

    auto const& chunk = *it;
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        std::cerr << "Failed to open " << outputPath << " for writing.\n";
        return 6;
    }

    if (!chunk.Data.empty())
        output.write(reinterpret_cast<char const*>(chunk.Data.data()), static_cast<std::streamsize>(chunk.Data.size()));

    std::cout << "Wrote " << chunk.Data.size() << " bytes to " << outputPath << ".\n";
    return 0;
}
