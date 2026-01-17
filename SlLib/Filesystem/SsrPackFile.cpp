#include "SsrPackFile.hpp"

#include "SlLib/Extensions/ByteReaderExtensions.hpp"
#include "SlLib/Utilities/SlUtil.hpp"

#include <algorithm>
#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#include <zlib.h>

namespace SlLib::Filesystem {

using namespace SlLib::Extensions;

namespace {

template <std::size_t N>
void writeInt32LE(std::array<std::uint8_t, N>& buffer, std::size_t offset, std::int32_t value)
{
    buffer[offset + 0] = static_cast<std::uint8_t>(value);
    buffer[offset + 1] = static_cast<std::uint8_t>(value >> 8);
    buffer[offset + 2] = static_cast<std::uint8_t>(value >> 16);
    buffer[offset + 3] = static_cast<std::uint8_t>(value >> 24);
}

std::vector<std::uint8_t> decompressZlib(std::span<const std::uint8_t> source, std::size_t destinationSize)
{
    std::vector<std::uint8_t> destination(destinationSize);

    z_stream stream{};
    stream.next_in = const_cast<std::uint8_t*>(source.data());
    stream.avail_in = static_cast<uInt>(source.size());
    stream.next_out = destination.data();
    stream.avail_out = static_cast<uInt>(destination.size());

    if (inflateInit(&stream) != Z_OK) {
        throw std::runtime_error("Failed to initialize zlib inflater.");
    }

    const int result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    if (result != Z_STREAM_END) {
        throw std::runtime_error("Failed to decompress SsrPackFile entry.");
    }

    return destination;
}

void writeEntry(std::fstream& stream, SsrPackFileEntry const& entry)
{
    std::array<std::uint8_t, 20> header{};
    writeInt32LE(header, 0, entry.FilenameHash);
    writeInt32LE(header, 4, static_cast<int>(entry.Offset));
    writeInt32LE(header, 8, entry.Size);
    writeInt32LE(header, 12, entry.CompressedSize);
    writeInt32LE(header, 16, entry.Flags);

    stream.seekp(entry.TempHackEntryFileOffset, std::ios::beg);
    stream.write(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!stream) {
        throw std::runtime_error("Failed to update SSR pack entry header.");
    }
}

} // namespace

SsrPackFile::SsrPackFile(std::filesystem::path path)
    : _path(std::move(path))
{
    if (!std::filesystem::exists(_path)) {
        throw std::runtime_error("SSR pack file not found: " + _path.string());
    }

    std::ifstream file(_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open SSR pack file: " + _path.string());
    }

    std::array<std::uint8_t, 24> header{};
    file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!file) {
        throw std::runtime_error("Failed to read SSR pack file header.");
    }

    std::vector<std::uint8_t> table;
    const int entryCount = Extensions::readInt32(header, 12);
    if (entryCount < 0) {
        throw std::runtime_error("Invalid SSR pack entry count.");
    }

    table.resize(static_cast<std::size_t>(entryCount) * 20);
    file.read(reinterpret_cast<char*>(table.data()), static_cast<std::streamsize>(table.size()));
    if (!file) {
        throw std::runtime_error("Failed to read SSR pack entry table.");
    }

    const std::span<const std::uint8_t> tableSpan(table);
    for (int i = 0; i < entryCount; ++i) {
        std::size_t offset = static_cast<std::size_t>(i * 20);
        SsrPackFileEntry entry;
        entry.FilenameHash = Extensions::readInt32(tableSpan, static_cast<int>(offset + 0));
        entry.Offset = static_cast<std::uint32_t>(Extensions::readInt32(tableSpan, static_cast<int>(offset + 4)));
        entry.Size = Extensions::readInt32(tableSpan, static_cast<int>(offset + 8));
        entry.CompressedSize = Extensions::readInt32(tableSpan, static_cast<int>(offset + 12));
        entry.Flags = Extensions::readInt32(tableSpan, static_cast<int>(offset + 16));
        entry.TempHackEntryFileOffset = 24 + static_cast<int>(offset);
        _entries.push_back(std::move(entry));
    }
}

bool SsrPackFile::DoesFileExist(std::string const& path) const
{
    const int hash = GetFilenameHash(path);
    return std::any_of(_entries.begin(), _entries.end(), [&](SsrPackFileEntry const& entry) {
        return entry.FilenameHash == hash;
    });
}

std::vector<std::uint8_t> SsrPackFile::GetFile(std::string const& path)
{
    SsrPackFileEntry& entry = GetFileEntry(path);
    std::ifstream file(_path, std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open SSR pack file for reading.");
    }

    file.seekg(entry.Offset, std::ios::beg);
    const std::size_t bytesToRead = static_cast<std::size_t>(entry.CompressedSize);
    std::vector<std::uint8_t> buffer(bytesToRead);
    file.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(bytesToRead));
    if (!file) {
        throw std::runtime_error("Failed to read SSR pack entry data.");
    }

    if (entry.CompressedSize != entry.Size) {
        return decompressZlib(buffer, static_cast<std::size_t>(entry.Size));
    }

    return buffer;
}

std::pair<std::unique_ptr<std::istream>, std::size_t> SsrPackFile::GetFileStream(std::string const& path)
{
    SsrPackFileEntry& entry = GetFileEntry(path);

    if (entry.CompressedSize != entry.Size) {
        std::vector<std::uint8_t> decompressed = GetFile(path);
        auto stream = std::make_unique<std::stringstream>(std::ios::binary);
        stream->write(reinterpret_cast<const char*>(decompressed.data()), static_cast<std::streamsize>(decompressed.size()));
        stream->seekg(0, std::ios::beg);
        return {std::move(stream), decompressed.size()};
    }

    auto stream = std::make_unique<std::ifstream>(_path, std::ios::binary);
    if (!stream || !*stream) {
        throw std::runtime_error("Failed to open SSR pack stream.");
    }

    stream->seekg(entry.Offset, std::ios::beg);
    return {std::move(stream), static_cast<std::size_t>(entry.Size)};
}

void SsrPackFile::AddFile(std::string path, std::vector<std::uint8_t> const& data)
{
    if (DoesFileExist(path)) {
        SetFile(path, data);
        return;
    }

    const std::size_t aligned = Utilities::align(data.size(), 0x800);
    std::vector<std::uint8_t> buffer(aligned);
    std::fill(buffer.begin(), buffer.end(), 0);
    std::copy(data.begin(), data.end(), buffer.begin());

    std::fstream file(_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("Failed to open SSR pack file for writing.");
    }

    file.seekp(0, std::ios::end);
    const std::uint32_t offset = static_cast<std::uint32_t>(file.tellp());
    file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
    if (!file) {
        throw std::runtime_error("Failed to append SSR pack entry.");
    }

    SsrPackFileEntry entry{};
    entry.FilenameHash = GetFilenameHash(path);
    entry.Offset = offset;
    entry.Size = static_cast<int>(data.size());
    entry.CompressedSize = static_cast<int>(data.size());
    entry.Flags = 0;
    entry.TempHackEntryFileOffset = 24 + static_cast<int>(_entries.size()) * 20;
    entry.Path = std::move(path);

    _entries.push_back(entry);

    writeEntry(file, _entries.back());

    const int entryCount = static_cast<int>(_entries.size());
    std::array<std::uint8_t, 4> countBuffer{};
    writeInt32LE(countBuffer, 0, entryCount);
    file.seekp(12, std::ios::beg);
    file.write(reinterpret_cast<char*>(countBuffer.data()), static_cast<std::streamsize>(countBuffer.size()));
}

void SsrPackFile::SetFile(std::string path, std::vector<std::uint8_t> const& data)
{
    SsrPackFileEntry& entry = GetFileEntry(path);
    std::fstream file(_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file) {
        throw std::runtime_error("Failed to open SSR pack file for updating.");
    }

    if (entry.CompressedSize >= static_cast<int>(data.size())) {
        file.seekp(entry.Offset, std::ios::beg);
        std::vector<std::uint8_t> buffer(entry.CompressedSize);
        std::fill(buffer.begin(), buffer.end(), 0);
        std::copy(data.begin(), data.end(), buffer.begin());
        file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        entry.Size = static_cast<int>(data.size());
        entry.CompressedSize = static_cast<int>(data.size());
    } else {
        const std::size_t aligned = Utilities::align(data.size(), 0x800);
        std::vector<std::uint8_t> buffer(aligned);
        std::fill(buffer.begin(), buffer.end(), 0);
        std::copy(data.begin(), data.end(), buffer.begin());

        file.seekp(0, std::ios::end);
        entry.Offset = static_cast<std::uint32_t>(file.tellp());
        file.write(reinterpret_cast<const char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
        entry.Size = static_cast<int>(data.size());
        entry.CompressedSize = static_cast<int>(data.size());
    }

    writeEntry(file, entry);
}

void SsrPackFile::Rebuild()
{
    int size = Utilities::align(24 + static_cast<int>(_entries.size()) * 20, 0x800);
    for (auto& entry : _entries) {
        entry.Offset = static_cast<std::uint32_t>(size);
        size = Utilities::align(size + entry.CompressedSize, 0x800);
    }
}

void SsrPackFile::Dispose()
{
}

SsrPackFileEntry& SsrPackFile::GetFileEntry(std::string const& path)
{
    const int hash = GetFilenameHash(path);
    auto it = std::find_if(_entries.begin(), _entries.end(),
                           [&](SsrPackFileEntry const& entry) { return entry.FilenameHash == hash; });

    if (it == _entries.end()) {
        throw std::runtime_error("SSR pack entry not found: " + path);
    }

    return *it;
}

std::int32_t SsrPackFile::GetFilenameHash(std::string const& input)
{
    std::string normalized = ".\\";
    normalized.reserve(input.size() + 2);
    for (char c : input) {
        char converted = (c == '/') ? '\\' : c;
        converted = static_cast<char>(std::toupper(static_cast<unsigned char>(converted)));
        normalized.push_back(converted);
    }

    std::uint32_t hash = 0;
    for (int i = static_cast<int>(normalized.size()) - 1; i >= 0; --i) {
        hash = hash * 0x83u + static_cast<unsigned char>(normalized[i]);
    }

    return static_cast<std::int32_t>(hash);
}

} // namespace SlLib::Filesystem
