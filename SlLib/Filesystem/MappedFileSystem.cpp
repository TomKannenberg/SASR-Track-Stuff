#include "MappedFileSystem.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace SlLib::Filesystem {

MappedFileSystem::MappedFileSystem(std::filesystem::path root)
    : _root(std::move(root))
{
    if (!std::filesystem::exists(_root)) {
        throw std::runtime_error("MappedFileSystem root does not exist: " + _root.string());
    }
}

bool MappedFileSystem::DoesFileExist(std::string const& path) const
{
    return std::filesystem::exists(_root / path);
}

std::vector<std::uint8_t> MappedFileSystem::GetFile(std::string const& path)
{
    const auto fullPath = _root / path;
    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("MappedFileSystem cannot read missing file: " + fullPath.string());
    }

    const auto fileSize = static_cast<std::size_t>(std::filesystem::file_size(fullPath));
    std::vector<std::uint8_t> buffer(fileSize);

    std::ifstream stream(fullPath, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("MappedFileSystem failed to open " + fullPath.string());
    }

    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!stream) {
        throw std::runtime_error("MappedFileSystem failed to read " + fullPath.string());
    }

    return buffer;
}

std::pair<std::unique_ptr<std::istream>, std::size_t> MappedFileSystem::GetFileStream(std::string const& path)
{
    const auto fullPath = _root / path;
    if (!std::filesystem::exists(fullPath)) {
        throw std::runtime_error("MappedFileSystem cannot stream missing file: " + fullPath.string());
    }

    auto stream = std::make_unique<std::ifstream>(fullPath, std::ios::binary);
    if (!stream || !*stream) {
        throw std::runtime_error("MappedFileSystem failed to open stream for " + fullPath.string());
    }

    stream->seekg(0, std::ios::end);
    const auto size = static_cast<std::size_t>(stream->tellg());
    stream->seekg(0, std::ios::beg);

    return {std::move(stream), size};
}

} // namespace SlLib::Filesystem
