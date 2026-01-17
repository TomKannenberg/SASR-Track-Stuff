#include "AkFilePackage.hpp"

#include "../Extensions/ByteReaderExtensions.hpp"

#include <cstddef>
#include <cstring>
#include <fstream>
#include <span>
#include <stdexcept>
#include <vector>

namespace SlLib::Audio {

namespace {

constexpr std::int32_t ExpectedFourCC = 0x4B504B41;
constexpr std::int32_t FileVersion = 1;

std::size_t toSize(std::int32_t value)
{
    if (value < 0) {
        throw std::runtime_error("AkFilePackage contains negative chunk size.");
    }

    return static_cast<std::size_t>(value);
}

template <typename EntryType>
void readChunk(std::span<const std::uint8_t> data, std::size_t chunkStart, std::size_t chunkSize,
               std::vector<EntryType>& destination)
{
    if (chunkSize < sizeof(std::int32_t)) {
        return;
    }

    if (chunkStart + chunkSize > data.size()) {
        throw std::runtime_error("AkFilePackage table is truncated.");
    }

    std::size_t cursor = chunkStart;
    std::int32_t entryCount = SlLib::Extensions::readInt32(data, cursor);
    cursor += sizeof(std::int32_t);

    if (entryCount < 0) {
        throw std::runtime_error("AkFilePackage contains an invalid entry count.");
    }

    std::size_t bytesNeeded = static_cast<std::size_t>(entryCount) * sizeof(EntryType);
    if (cursor + bytesNeeded > chunkStart + chunkSize) {
        throw std::runtime_error("AkFilePackage entry table is truncated.");
    }

    destination.resize(static_cast<std::size_t>(entryCount));
    if (bytesNeeded > 0) {
        std::memcpy(destination.data(), data.data() + cursor, bytesNeeded);
    }
}

} // namespace

AkFilePackage AkFilePackage::Load(std::filesystem::path const& path)
{
    const auto pathString = path.string();
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error("Could not find AkFilePackage at " + pathString);
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open AkFilePackage at " + pathString);
    }

    AkFileHeader header{};
    input.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (!input) {
        throw std::runtime_error("Failed to read AkFilePackage header.");
    }

    if (header.FourCC != ExpectedFourCC) {
        throw std::runtime_error("Not a valid AkFilePackage; fourCC mismatch.");
    }

    if (header.Version != FileVersion) {
        throw std::runtime_error("Unsupported AkFilePackage version.");
    }

    const std::size_t tableSize = toSize(header.LanguageMapSize) + toSize(header.SoundBanksTableSize) +
                                  toSize(header.StreamingFilesTableSize) + toSize(header.ExternalsTableSize);
    std::vector<std::uint8_t> tableData(tableSize);
    if (tableSize > 0) {
        input.read(reinterpret_cast<char*>(tableData.data()), static_cast<std::streamsize>(tableSize));
        if (!input) {
            throw std::runtime_error("Failed to read AkFilePackage string table.");
        }
    }

    std::span<const std::uint8_t> tableSpan(tableData);
    if (tableSpan.size() < sizeof(std::int32_t)) {
        throw std::runtime_error("AkFilePackage string table is too small.");
    }

    AkFilePackage package;

    std::size_t cursor = 0;
    std::int32_t stringCount = SlLib::Extensions::readInt32(tableSpan, cursor);
    cursor += sizeof(std::int32_t);

    for (std::int32_t i = 0; i < stringCount; ++i) {
        std::int32_t offset = SlLib::Extensions::readInt32(tableSpan, cursor);
        cursor += sizeof(std::int32_t);

        std::int32_t id = SlLib::Extensions::readInt32(tableSpan, cursor);
        cursor += sizeof(std::int32_t);

        package.LanguageMap[id] =
            SlLib::Extensions::readTerminatedUnicodeString(tableSpan, static_cast<std::size_t>(offset));
    }

    std::size_t chunkStart = toSize(header.LanguageMapSize);
    readChunk(tableSpan, chunkStart, toSize(header.SoundBanksTableSize), package.SoundBanks);
    chunkStart += toSize(header.SoundBanksTableSize);

    readChunk(tableSpan, chunkStart, toSize(header.StreamingFilesTableSize), package.StreamedFiles);
    chunkStart += toSize(header.StreamingFilesTableSize);

    readChunk(tableSpan, chunkStart, toSize(header.ExternalsTableSize), package.Externals);

    return package;
}

} // namespace SlLib::Audio
