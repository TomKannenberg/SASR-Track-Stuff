#include "SifParser.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

#include <zlib.h>

namespace SeEditor {

namespace {

constexpr std::uint32_t MakeTypeCode(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

std::uint32_t ReadUint32(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

std::uint32_t ReadUint32BE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[3]) |
           (static_cast<std::uint32_t>(ptr[2]) << 8) |
           (static_cast<std::uint32_t>(ptr[1]) << 16) |
           (static_cast<std::uint32_t>(ptr[0]) << 24);
}

bool LooksLikeZlib(std::span<const std::uint8_t> data)
{
    if (data.size() < 2)
        return false;
    std::uint8_t cmf = data[0];
    std::uint8_t flg = data[1];
    if ((cmf & 0x0F) != 8)
        return false;
    return (((static_cast<int>(cmf) << 8) | flg) % 31) == 0;
}

std::vector<std::uint8_t> DecompressZlib(std::span<const std::uint8_t> stream)
{
    z_stream inflater{};
    inflater.next_in = const_cast<std::uint8_t*>(stream.data());
    inflater.avail_in = static_cast<decltype(inflater.avail_in)>(stream.size());

    constexpr int kZlibOkValue = Z_OK;
    constexpr int kZlibStreamEndValue = Z_STREAM_END;
    constexpr int kZlibFlushNone = 0;

    if (inflateInit(&inflater) != kZlibOkValue)
        throw std::runtime_error("Failed to init zlib inflater.");

    std::vector<std::uint8_t> result;
    std::vector<std::uint8_t> buffer(1 << 14);

    int status = kZlibOkValue;
    while (status != kZlibStreamEndValue)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<decltype(inflater.avail_out)>(buffer.size());

        status = inflate(&inflater, kZlibFlushNone);
        if (status != kZlibOkValue && status != kZlibStreamEndValue)
        {
            inflateEnd(&inflater);
            throw std::runtime_error("SIF decompression failure.");
        }

        size_t have = buffer.size() - inflater.avail_out;
        result.insert(result.end(), buffer.begin(), buffer.begin() + have);
    }

    inflateEnd(&inflater);
    return result;
}

std::string ResourceTypeName(std::uint32_t value)
{
    switch (value)
    {
    case 0x4F464E49: return "Info";
    case 0x58455450: return "TexturePack";
    case 0x4D52464B: return "KeyFrameLibrary";
    case 0x534A424F: return "ObjectDefLibrary";
    case 0x454E4353: return "SceneLibrary";
    case 0x544E4F46: return "FontPack";
    case 0x54584554: return "TextPack";
    case 0x4F4C4552: return "Relocations";
    case 0x45524F46: return "Forest";
    case 0x4B415254: return "Navigation";
    case 0x4C415254: return "Trail";
    case 0x43474F4C: return "Logic";
    case 0x34445650: return "VisData";
    case 0x31304853: return "ShData";
    case 0x3220464C: return "LensFlare2";
    case 0x3120464C: return "LensFlare1";
    case 0x494C4F43: return "Collision";
    default: return "Unknown";
    }
}

} // namespace

std::optional<SifParseResult> ParseSifFile(std::span<const std::uint8_t> raw, std::string& error)
{
    SifParseResult result;
    std::vector<std::uint8_t> working(raw.begin(), raw.end());

    if (LooksLikeZlib(raw))
    {
        result.WasCompressed = true;
        try
        {
            std::vector<std::uint8_t> decompressed = DecompressZlib(raw);
            if (decompressed.size() < 4)
            {
                error = "Compressed stream missing header.";
                return std::nullopt;
            }

            std::uint32_t expectedLength = ReadUint32(decompressed.data());
            working.assign(decompressed.begin() + 4, decompressed.end());
            result.DecompressedSize = working.size();
            if (expectedLength != result.DecompressedSize)
                std::cout << "[CharmyBee] Warning: compressed header length "
                          << expectedLength << " differs from actual " << result.DecompressedSize << '.' << std::endl;
        }
        catch (std::runtime_error const& ex)
        {
            error = ex.what();
            return std::nullopt;
        }
    }

    if (result.DecompressedSize == 0)
        result.DecompressedSize = working.size();

    if (working.empty())
    {
        error = "SIF payload is empty.";
        return std::nullopt;
    }

    const std::uint8_t* data = working.data();
    std::size_t size = working.size();

    std::size_t offset = 0;
    if (size >= 8)
    {
        std::uint32_t possibleLength = ReadUint32(data);
        if (possibleLength == size - 4 || possibleLength == size)
            offset = 4; // some SIF files start with a total-length prefix
    }

    while (offset + 0x10 <= size)
    {
        const std::uint8_t* header = data + offset;
        std::uint32_t type = ReadUint32(header);
        std::uint32_t endianMarkerLE = ReadUint32(header + 12);
        bool bigEndian = endianMarkerLE == 0x11223344;
        auto read32 = [&](const std::uint8_t* ptr) {
            return bigEndian ? ReadUint32BE(ptr) : ReadUint32(ptr);
        };
        auto read16 = [&](const std::uint8_t* ptr) {
            return bigEndian ? static_cast<std::uint16_t>((ptr[0] << 8) | ptr[1])
                             : static_cast<std::uint16_t>(ptr[0] | (ptr[1] << 8));
        };

        auto isChunkRangeValid = [&](std::uint32_t cs, std::uint32_t ds) {
            return cs >= 0x10 && cs <= size - offset && ds <= cs - 0x10;
        };

        std::uint32_t chunkSize = read32(header + 4);
        std::uint32_t dataSize = read32(header + 8);

        if (!isChunkRangeValid(chunkSize, dataSize) && !bigEndian)
        {
            std::uint32_t chunkSizeBE = ReadUint32BE(header + 4);
            std::uint32_t dataSizeBE = ReadUint32BE(header + 8);
            if (isChunkRangeValid(chunkSizeBE, dataSizeBE))
            {
                bigEndian = true;
                chunkSize = chunkSizeBE;
                dataSize = dataSizeBE;
            }
        }

        if (chunkSize < 0x10 || chunkSize > size - offset)
        {
            error = "Invalid SIF chunk size.";
            return std::nullopt;
        }

        if (dataSize > chunkSize - 0x10)
        {
            error = "SIF chunk data exceeds chunk header.";
            return std::nullopt;
        }

        SifChunkInfo chunk;
        chunk.TypeValue = type;
        chunk.Name = ResourceTypeName(type);
        chunk.DataSize = dataSize;
        chunk.ChunkSize = chunkSize;
        chunk.BigEndian = bigEndian;
        chunk.Data.assign(header + 0x10, header + 0x10 + dataSize);
        chunk.RawChunk.assign(header, header + chunkSize);

        offset += chunkSize;

        if (offset + 0x10 <= size)
        {
            const std::uint8_t* relocHeader = data + offset;
            std::uint32_t relocType = ReadUint32(relocHeader);
            if (relocType == MakeTypeCode('R', 'E', 'L', 'O'))
            {
                std::uint32_t relocSize = read32(relocHeader + 4);
                std::uint32_t relocDataSize = read32(relocHeader + 8);
                chunk.RelocChunkSize = relocSize;
                chunk.RelocDataSize = relocDataSize;

                if (relocSize >= 0x10 && relocSize <= size - offset && relocDataSize >= 4 &&
                    relocSize >= 0x10 + relocDataSize)
                {
                    const std::uint8_t* relocData = relocHeader + 0x10;
                    if (ReadUint32(relocData) == type)
                    {
                        std::size_t cursor = 4;
                        while (cursor + 8 <= relocDataSize)
                        {
                            std::uint16_t flag = read16(relocData + cursor);
                            cursor += 4;
                            std::uint32_t relocateAddress = read32(relocData + cursor);
                            cursor += 4;

                            if (flag == 1)
                                chunk.Relocations.push_back(relocateAddress);
                            if (flag != 1)
                                break;
                        }
                    }
                }

                if (relocSize >= 0x10 && relocSize <= size - offset)
                    chunk.RelocRaw.assign(relocHeader, relocHeader + relocSize);

                offset += relocSize;
            }
        }

        result.Chunks.push_back(std::move(chunk));
    }

    return result;
}

} // namespace SeEditor
