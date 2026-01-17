#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <memory>
#include <string>
#include <vector>

namespace SlLib::Filesystem {

class IFileSystem
{
public:
    virtual ~IFileSystem() = default;

    virtual bool DoesFileExist(std::string const& path) const = 0;
    virtual std::vector<std::uint8_t> GetFile(std::string const& path) = 0;
    virtual std::pair<std::unique_ptr<std::istream>, std::size_t> GetFileStream(
        std::string const& path) = 0;
};

} // namespace SlLib::Filesystem
