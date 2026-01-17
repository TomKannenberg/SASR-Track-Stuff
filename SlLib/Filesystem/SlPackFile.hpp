#pragma once

#include "IFileSystem.hpp"
#include "SlPackFileTocEntry.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace SlLib::Filesystem {

class SlPackFile final : public IFileSystem
{
public:
    explicit SlPackFile(std::filesystem::path path);

    bool DoesFileExist(std::string const& path) const override;
    std::vector<std::uint8_t> GetFile(std::string const& path) override;
    std::pair<std::unique_ptr<std::istream>, std::size_t> GetFileStream(std::string const& path) override;

private:
    SlPackFileTocEntry& GetFileEntry(std::string const& path);
    void ResolveEntryPaths(SlPackFileTocEntry& entry);

    std::vector<SlPackFileTocEntry> _entries;
    std::vector<std::filesystem::path> _packs;
};

} // namespace SlLib::Filesystem
