#include "SlPackFile.hpp"

#include "SlLib/Extensions/ByteReaderExtensions.hpp"
#include "SlLib/Utilities/CryptUtil.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

using namespace SlLib::Extensions;
using namespace SlLib::Utilities;

namespace SlLib::Filesystem {

namespace {

std::filesystem::path FormatPackFile(std::filesystem::path const& base, int index)
{
    std::ostringstream buffer;
    buffer << base.string() << ".M" << std::setw(2) << std::setfill('0') << index;
    return buffer.str();
}

} // namespace

SlPackFile::SlPackFile(std::filesystem::path path)
{
    path.replace_extension("");
    const std::filesystem::path tocPath = path.string() + ".toc";

    std::ifstream tocStream(tocPath, std::ios::binary);
    if (!tocStream)
        throw std::runtime_error("Failed to open pack file TOC: " + tocPath.string());

    const std::size_t size = static_cast<std::size_t>(std::filesystem::file_size(tocPath));
    std::vector<std::uint8_t> tocData(size);
    tocStream.read(reinterpret_cast<char*>(tocData.data()), static_cast<std::streamsize>(size));
    if (!tocStream)
        throw std::runtime_error("Failed to read pack file TOC: " + tocPath.string());

    CryptUtil::PackFileUnmunge(std::span(tocData));
    std::span<const std::uint8_t> tocSpan(tocData);

    const int binCount = readInt32(tocSpan, 0x10);
    const int tableOffset = readInt32(tocSpan, 0x14);
    const int stringsOffset = readInt32(tocSpan, 0x18);

    for (int i = 0; i < binCount; ++i) {
        const auto packPath = FormatPackFile(path, i);
        if (!std::filesystem::exists(packPath))
            throw std::runtime_error("Missing pack file: " + packPath.string());
        _packs.push_back(packPath);
    }

    std::size_t offset = static_cast<std::size_t>(tableOffset);
    const std::size_t stringsBegin = static_cast<std::size_t>(stringsOffset);

    while (offset + 24 <= tocSpan.size() && offset + 24 <= stringsBegin) {
        SlPackFileTocEntry entry;
        entry.Hash = readInt32(tocSpan, static_cast<int>(offset + 0));
        entry.IsDirectory = tocSpan[offset + 4] != 0;
        entry.Parent = readInt32(tocSpan, static_cast<int>(offset + 8));
        entry.Offset = static_cast<std::uint32_t>(readInt32(tocSpan, static_cast<int>(offset + 12)));
        entry.Size = readInt32(tocSpan, static_cast<int>(offset + 16));

        const int namePointer = readInt32(tocSpan, static_cast<int>(offset + 20));
        const std::size_t nameOffset = stringsBegin + static_cast<std::size_t>(namePointer);
        if (nameOffset >= tocSpan.size())
            throw std::runtime_error("Pack file TOC name offset out of bounds.");

        entry.Name = readString(tocSpan, static_cast<int>(nameOffset));
        entry.Path = entry.Name;
        _entries.push_back(std::move(entry));
        offset += 24;
    }

    if (_entries.empty())
        throw std::runtime_error("Pack file TOC contained no entries.");

    ResolveEntryPaths(_entries[0]);
}

bool SlPackFile::DoesFileExist(std::string const& path) const
{
    return std::any_of(_entries.begin(), _entries.end(), [&](SlPackFileTocEntry const& entry) {
        return !entry.IsDirectory && entry.Path == path;
    });
}

std::vector<std::uint8_t> SlPackFile::GetFile(std::string const& path)
{
    SlPackFileTocEntry& entry = GetFileEntry(path);

    auto stream = std::ifstream(_packs.at(entry.Parent), std::ios::binary);
    if (!stream) {
        throw std::runtime_error("Failed to open pack data file.");
    }

    stream.seekg(entry.Offset, std::ios::beg);
    std::vector<std::uint8_t> buffer(entry.Size);
    stream.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!stream) {
        throw std::runtime_error("Failed to read pack entry data.");
    }

    return buffer;
}

std::pair<std::unique_ptr<std::istream>, std::size_t> SlPackFile::GetFileStream(std::string const& path)
{
    SlPackFileTocEntry& entry = GetFileEntry(path);
    auto stream = std::make_unique<std::ifstream>(_packs.at(entry.Parent), std::ios::binary);
    if (!stream || !*stream)
        throw std::runtime_error("Failed to open pack data stream.");

    stream->seekg(entry.Offset, std::ios::beg);
    return {std::move(stream), static_cast<std::size_t>(entry.Size)};
}

SlPackFileTocEntry& SlPackFile::GetFileEntry(std::string const& path)
{
    auto it = std::find_if(_entries.begin(), _entries.end(), [&](SlPackFileTocEntry& entry) {
        return !entry.IsDirectory && entry.Path == path;
    });

    if (it == _entries.end()) {
        throw std::runtime_error("Pack entry not found: " + path);
    }

    return *it;
}

void SlPackFile::ResolveEntryPaths(SlPackFileTocEntry& entry)
{
    if (!entry.IsDirectory) {
        return;
    }

    const bool isRoot = entry.Parent == -1;
    const std::size_t start = entry.Offset;
    const std::size_t end = entry.Offset + static_cast<std::size_t>(entry.Size);

    if (start >= _entries.size()) {
        return;
    }

    for (std::size_t i = start; i < end && i < _entries.size(); ++i) {
        SlPackFileTocEntry& child = _entries[i];
        if (!isRoot && !entry.Path.empty()) {
            child.Path = entry.Path.empty() ? child.Name : entry.Path + "/" + child.Path;
        } else if (!isRoot && child.Path.empty()) {
            child.Path = child.Name;
        }

        if (child.IsDirectory) {
            ResolveEntryPaths(child);
        }
    }
}

} // namespace SlLib::Filesystem
