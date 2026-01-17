#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace SlLib::Audio {

template <typename TFileId>
struct AkFileEntry
{
    TFileId FileId;
    std::int32_t BlockSize;
    std::int32_t FileSize;
    std::int32_t StartBlock;
    std::int32_t LanguageId;
};

struct AkFileHeader
{
    std::int32_t FourCC;
    std::int32_t HeaderSize;
    std::int32_t Version;
    std::int32_t LanguageMapSize;
    std::int32_t SoundBanksTableSize;
    std::int32_t StreamingFilesTableSize;
    std::int32_t ExternalsTableSize;
};

class AkFilePackage final
{
public:
    static AkFilePackage Load(std::filesystem::path const& path);

    std::unordered_map<std::int32_t, std::string> LanguageMap;
    std::vector<AkFileEntry<std::int32_t>> SoundBanks;
    std::vector<AkFileEntry<std::int32_t>> StreamedFiles;
    std::vector<AkFileEntry<std::int64_t>> Externals;
};

} // namespace SlLib::Audio
