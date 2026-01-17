#pragma once

#include "IFileSystem.hpp"
#include "SsrPackFileEntry.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace SlLib::Filesystem {

class SsrPackFile final : public IFileSystem
{
public:
    explicit SsrPackFile(std::filesystem::path path);

    bool DoesFileExist(std::string const& path) const override;
    std::vector<std::uint8_t> GetFile(std::string const& path) override;
    std::pair<std::unique_ptr<std::istream>, std::size_t> GetFileStream(std::string const& path) override;

    void AddFile(std::string path, std::vector<std::uint8_t> const& data);
    void SetFile(std::string path, std::vector<std::uint8_t> const& data);
    void Rebuild();
    void Dispose();

private:
    SsrPackFileEntry& GetFileEntry(std::string const& path);
    static std::int32_t GetFilenameHash(std::string const& input);

    std::vector<SsrPackFileEntry> _entries;
    std::filesystem::path _path;
};

} // namespace SlLib::Filesystem
