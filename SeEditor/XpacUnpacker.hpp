#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace SeEditor::Xpac {

struct XpacEntry
{
    std::uint32_t Hash = 0;
    std::uint32_t Offset = 0;
    std::uint32_t Size = 0;
    std::uint32_t CompressedSize = 0;
    std::uint32_t Flags = 0;
};

struct XpacUnpackOptions
{
    std::filesystem::path XpacPath;
    std::filesystem::path OutputRoot;
    std::optional<std::filesystem::path> MappingPath;
    bool ConvertToSifSig = true;
    std::function<void(std::size_t current, std::size_t total)> Progress;
    std::function<void(std::size_t current, std::size_t total)> ProgressConvert;
};

struct XpacUnpackResult
{
    std::size_t TotalEntries = 0;
    std::size_t ExtractedZif = 0;
    std::size_t ExtractedZig = 0;
    std::size_t ConvertedPairs = 0;
    std::size_t SkippedEntries = 0;
    std::vector<std::string> Errors;
};

std::optional<std::filesystem::path> FindDefaultMappingPath(std::filesystem::path const& xpacPath,
                                                            std::filesystem::path const& outputRoot);

XpacUnpackResult UnpackXpac(XpacUnpackOptions const& options);

} // namespace SeEditor::Xpac
