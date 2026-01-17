#pragma once

#include "IFileSystem.hpp"

#include <filesystem>
#include <string>

namespace SlLib::Filesystem {

class MappedFileSystem final : public IFileSystem
{
public:
    explicit MappedFileSystem(std::filesystem::path root);

    bool DoesFileExist(std::string const& path) const override;
    std::vector<std::uint8_t> GetFile(std::string const& path) override;
    std::pair<std::unique_ptr<std::istream>, std::size_t> GetFileStream(std::string const& path) override;

private:
    std::filesystem::path _root;
};

} // namespace SlLib::Filesystem
