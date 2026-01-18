#include "XpacUnpacker.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <unordered_map>
#include <span>

#ifdef Z_SOLO
#undef Z_SOLO
#endif
#include <zlib.h>

namespace SeEditor::Xpac {

namespace {

constexpr std::uint32_t MakeTypeCode(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

std::uint32_t ReadU32LE(std::span<const std::uint8_t> data, std::size_t offset)
{
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
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

bool LooksLikeSif(std::span<const std::uint8_t> data)
{
    if (data.size() < 0x10)
        return false;

    std::size_t offset = 0;
    std::uint32_t possibleLength = ReadU32LE(data, 0);
    if (possibleLength == data.size() - 4 || possibleLength == data.size())
        offset = 4;

    if (offset + 0x10 > data.size())
        return false;

    std::uint32_t type = ReadU32LE(data, offset);
    switch (type)
    {
    case MakeTypeCode('T', 'R', 'A', 'K'):
    case MakeTypeCode('F', 'O', 'R', 'E'):
    case MakeTypeCode('C', 'O', 'L', 'I'):
    case MakeTypeCode('L', 'O', 'G', 'C'):
    case MakeTypeCode('P', 'T', 'E', 'X'):
    case MakeTypeCode('I', 'N', 'F', 'O'):
        return true;
    default:
        return false;
    }
}

std::vector<std::uint8_t> DecompressZlib(std::span<const std::uint8_t> data, std::size_t expectedSize)
{
    constexpr int kZlibBufError = -5;
    z_stream inflater{};
    inflater.next_in = const_cast<std::uint8_t*>(data.data());
    inflater.avail_in = static_cast<uInt>(data.size());

    if (inflateInit(&inflater) != Z_OK)
        throw std::runtime_error("Failed to init zlib inflater.");

    std::vector<std::uint8_t> result;
    if (expectedSize != 0)
        result.reserve(expectedSize);

    std::array<std::uint8_t, 16 * 1024> buffer{};
    int status = Z_OK;
    static bool loggedFirstRun = false;
    while (status != Z_STREAM_END)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<uInt>(buffer.size());
        status = inflate(&inflater, 0);
        if (!loggedFirstRun)
        {
            loggedFirstRun = true;
            std::cerr << "[XPAC] inflate status=" << status
                      << " avail_in=" << inflater.avail_in
                      << " avail_out=" << inflater.avail_out << "\n";
        }
        if (status != Z_OK && status != Z_STREAM_END && status != kZlibBufError)
        {
            inflateEnd(&inflater);
            throw std::runtime_error("XPAC zlib decompression failed.");
        }

        std::size_t have = buffer.size() - inflater.avail_out;
        if (have > 0)
            result.insert(result.end(), buffer.begin(), buffer.begin() + have);
        if (status == kZlibBufError && inflater.avail_in == 0 && have == 0)
            break;
    }

    inflateEnd(&inflater);
    return result;
}

std::unordered_map<std::uint32_t, std::string> LoadMapping(std::filesystem::path const& mappingPath)
{
    std::unordered_map<std::uint32_t, std::string> mapping;
    std::ifstream file(mappingPath);
    if (!file)
        return mapping;

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;
        auto colon = line.find(':');
        auto semi = line.find(';', colon != std::string::npos ? colon + 1 : 0);
        if (colon == std::string::npos || semi == std::string::npos)
            continue;
        std::string key = line.substr(0, colon);
        std::string value = line.substr(colon + 1, semi - colon - 1);
        try
        {
            std::uint32_t hash = static_cast<std::uint32_t>(std::stoul(key));
            mapping.emplace(hash, value);
        }
        catch (...)
        {
            continue;
        }
    }

    return mapping;
}

std::filesystem::path CleanMappingPath(std::string value)
{
    for (char& ch : value)
    {
        if (ch == '\\')
            ch = '/';
    }
    if (value.rfind("./", 0) == 0)
        value.erase(0, 2);
    if (value.rfind(".\\", 0) == 0)
        value.erase(0, 2);
    while (!value.empty() && (value.front() == '/' || value.front() == '\\'))
        value.erase(value.begin());

    for (char& ch : value)
    {
        if (ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
            ch = '_';
    }

    return std::filesystem::path(value);
}

std::filesystem::path BuildXpacToolRelativePath(std::string const& mappedName)
{
    std::filesystem::path cleaned = CleanMappingPath(mappedName);
    std::filesystem::path current;
    for (auto const& part : cleaned)
    {
        std::string segment = part.string();
        if (segment.find('.') != std::string::npos)
        {
            current /= segment;
            break;
        }
        current /= "_" + segment;
    }
    return current;
}

std::string HashHex(std::uint32_t hash)
{
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setfill('0') << std::setw(8) << hash;
    return oss.str();
}

bool WriteFile(std::filesystem::path const& path, std::span<const std::uint8_t> data, std::string& error)
{
    try
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream out(path, std::ios::binary);
        if (!out)
        {
            error = "Failed to open output file " + path.string();
            return false;
        }
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
        return true;
    }
    catch (std::exception const& ex)
    {
        error = std::string("Failed to write file: ") + ex.what();
        return false;
    }
}

bool DecodeZifZig(std::span<const std::uint8_t> data, std::vector<std::uint8_t>& out, std::string& error)
{
    if (data.empty())
    {
        error = "ZIF/ZIG payload is empty.";
        return false;
    }

    auto stripLengthPrefix = [&](std::span<const std::uint8_t> buffer, std::vector<std::uint8_t>& target) {
        if (buffer.size() < 4)
        {
            target.assign(buffer.begin(), buffer.end());
            return;
        }

        std::uint32_t expected = ReadU32LE(buffer, 0);
        std::size_t payloadSize = buffer.size() - 4;
        if (expected > 0 && expected <= payloadSize)
        {
            target.assign(buffer.begin() + 4, buffer.begin() + 4 + expected);
            return;
        }

        target.assign(buffer.begin(), buffer.end());
    };

    if (!LooksLikeZlib(data))
    {
        stripLengthPrefix(data, out);
        return true;
    }

    std::vector<std::uint8_t> decompressed = DecompressZlib(data, 0);
    if (decompressed.empty())
    {
        error = "Zlib decompress produced no output.";
        return false;
    }

    stripLengthPrefix(decompressed, out);
    return true;
}

bool ReadFileBytes(std::filesystem::path const& path, std::vector<std::uint8_t>& out, std::string& error)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        error = "Unable to open file " + path.string();
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(file), {});
    return true;
}

} // namespace

std::optional<std::filesystem::path> FindDefaultMappingPath(std::filesystem::path const& xpacPath,
                                                            std::filesystem::path const& outputRoot)
{
    std::vector<std::filesystem::path> candidates;
    if (!xpacPath.empty())
    {
        candidates.push_back(xpacPath.parent_path() / "MAPPING.GC");
        candidates.push_back(xpacPath.parent_path() / "MAPPINGS.GC");
    }
    candidates.push_back(std::filesystem::current_path() / "MAPPING.GC");
    candidates.push_back(std::filesystem::current_path() / "MAPPINGS.GC");
    candidates.push_back(outputRoot / "MAPPING.GC");
    candidates.push_back(outputRoot / "MAPPINGS.GC");

    std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    candidates.push_back(repoRoot / "xpac-Tool" / "MAPPING.GC");
    candidates.push_back(repoRoot / "xpac-Tool" / "MAPPINGS.GC");

    for (auto const& candidate : candidates)
    {
        if (!candidate.empty() && std::filesystem::exists(candidate))
            return candidate;
    }
    return std::nullopt;
}

XpacUnpackResult UnpackXpac(XpacUnpackOptions const& options)
{
    XpacUnpackResult result;
    if (options.XpacPath.empty())
    {
        result.Errors.push_back("XPAC path missing.");
        return result;
    }

    std::filesystem::path outputRoot = options.OutputRoot;
    if (outputRoot.empty())
    {
        result.Errors.push_back("Output root missing.");
        return result;
    }

    std::error_code ec;
    std::filesystem::create_directories(outputRoot, ec);
    if (ec)
    {
        result.Errors.push_back("Failed to create output directory: " + outputRoot.string());
        return result;
    }

    std::optional<std::filesystem::path> mappingPath = options.MappingPath;
    if (!mappingPath)
        mappingPath = FindDefaultMappingPath(options.XpacPath, outputRoot);
    std::unordered_map<std::uint32_t, std::string> mapping;
    if (mappingPath)
        mapping = LoadMapping(*mappingPath);

    std::ifstream file(options.XpacPath, std::ios::binary);
    if (!file)
    {
        result.Errors.push_back("Unable to open XPAC file: " + options.XpacPath.string());
        return result;
    }

    file.seekg(0, std::ios::end);
    std::uint64_t fileSize = static_cast<std::uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::array<std::uint8_t, 24> header{};
    if (!file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())))
    {
        result.Errors.push_back("Failed to read XPAC header.");
        return result;
    }

    std::uint32_t totalFiles = ReadU32LE(header, 12);
    result.TotalEntries = totalFiles;

    std::vector<XpacEntry> entries;
    entries.reserve(totalFiles);
    for (std::uint32_t i = 0; i < totalFiles; ++i)
    {
        std::array<std::uint8_t, 20> entryBuf{};
        if (!file.read(reinterpret_cast<char*>(entryBuf.data()), static_cast<std::streamsize>(entryBuf.size())))
        {
            result.Errors.push_back("Failed to read XPAC entry table.");
            return result;
        }
        XpacEntry entry;
        entry.Hash = ReadU32LE(entryBuf, 0);
        entry.Offset = ReadU32LE(entryBuf, 4);
        entry.Size = ReadU32LE(entryBuf, 8);
        entry.CompressedSize = ReadU32LE(entryBuf, 12);
        entry.Flags = ReadU32LE(entryBuf, 16);
        entries.push_back(entry);
    }

    std::filesystem::path xpacBase = options.XpacPath.stem();
    std::filesystem::path xpacRoot = outputRoot / "xpac" / xpacBase;
    std::filesystem::create_directories(xpacRoot, ec);
    if (ec)
    {
        result.Errors.push_back("Failed to create XPAC output directory: " + xpacRoot.string());
        return result;
    }

    std::filesystem::path manifestPath = xpacRoot / "manifest.csv";
    std::ofstream manifest(manifestPath);
    if (manifest)
        manifest << "hash_hex,hash_dec,name,size,compressed_size,flags,output_path,is_sif\n";

    struct PairPaths
    {
        std::filesystem::path Zif;
        std::filesystem::path Zig;
    };
    std::unordered_map<std::string, PairPaths> pairMap;
    std::size_t processed = 0;

    for (auto const& entry : entries)
    {
        if (options.Progress)
            options.Progress(processed, entries.size());

        std::uint64_t endOffset = static_cast<std::uint64_t>(entry.Offset) + entry.CompressedSize;
        if (endOffset > fileSize)
        {
            result.SkippedEntries++;
            result.Errors.push_back("Entry out of bounds for hash " + HashHex(entry.Hash));
            continue;
        }

        std::vector<std::uint8_t> compressed(entry.CompressedSize);
        file.seekg(entry.Offset, std::ios::beg);
        if (!file.read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(compressed.size())))
        {
            result.SkippedEntries++;
            result.Errors.push_back("Failed to read entry data for hash " + HashHex(entry.Hash));
            continue;
        }

        std::vector<std::uint8_t> payload;
        try
        {
            if (entry.CompressedSize != entry.Size)
                payload = DecompressZlib(compressed, entry.Size);
            else
                payload = std::move(compressed);
        }
        catch (std::exception const& ex)
        {
            result.SkippedEntries++;
            result.Errors.push_back("Decompression failed for hash " + HashHex(entry.Hash) + ": " + ex.what());
            continue;
        }

        std::string mappedName;
        auto it = mapping.find(entry.Hash);
        if (it != mapping.end())
            mappedName = it->second;

        std::filesystem::path relativePath;
        if (!mappedName.empty())
            relativePath = BuildXpacToolRelativePath(mappedName);

        bool isSif = LooksLikeSif(payload);
        std::filesystem::path outputPath;
        std::string extension;
        if (!relativePath.empty())
        {
            extension = relativePath.extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                           [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        }

        bool wroteFile = false;
        if (!extension.empty() && (extension == ".zif" || extension == ".zig"))
        {
            outputPath = xpacRoot / relativePath;
            std::string error;
            if (WriteFile(outputPath, payload, error))
            {
                wroteFile = true;
                if (extension == ".zif")
                    result.ExtractedZif++;
                else
                    result.ExtractedZig++;

                std::filesystem::path base = outputPath;
                base.replace_extension();
                PairPaths& pair = pairMap[base.string()];
                if (extension == ".zif")
                    pair.Zif = outputPath;
                else
                    pair.Zig = outputPath;
            }
            else
            {
                result.Errors.push_back(error);
            }
        }
        else if (isSif)
        {
            std::filesystem::path unknownDir = xpacRoot / "unknown_cpu";
            std::filesystem::path name = "hash_" + HashHex(entry.Hash) + ".zif";
            outputPath = unknownDir / name;
            std::string error;
            if (WriteFile(outputPath, payload, error))
            {
                wroteFile = true;
                result.ExtractedZif++;
            }
            else
            {
                result.Errors.push_back(error);
            }
        }
        else
        {
            result.SkippedEntries++;
        }

        if (manifest && wroteFile)
        {
            manifest << HashHex(entry.Hash) << ",";
            manifest << entry.Hash << ",";
            manifest << (mappedName.empty() ? "" : mappedName) << ",";
            manifest << entry.Size << ",";
            manifest << entry.CompressedSize << ",";
            manifest << entry.Flags << ",";
            manifest << outputPath.string() << ",";
            manifest << (isSif ? "1" : "0") << "\n";
        }

        ++processed;
    }

    if (options.ConvertToSifSig)
    {
        for (auto const& [base, pair] : pairMap)
        {
            if (pair.Zif.empty() || pair.Zig.empty())
                continue;

            std::vector<std::uint8_t> sif;
            std::vector<std::uint8_t> sig;
            std::string error;
            std::vector<std::uint8_t> zifBytes;
            if (!ReadFileBytes(pair.Zif, zifBytes, error) ||
                !DecodeZifZig(zifBytes, sif, error))
            {
                result.Errors.push_back("Failed to convert " + pair.Zif.string() + ": " + error);
                continue;
            }
            if (sif.empty())
            {
                std::ostringstream message;
                message << "Decoded empty SIF: " << pair.Zif.string()
                        << " (input " << zifBytes.size() << " bytes)";
                result.Errors.push_back(message.str());
                continue;
            }

            std::vector<std::uint8_t> zigBytes;
            if (!ReadFileBytes(pair.Zig, zigBytes, error) ||
                !DecodeZifZig(zigBytes, sig, error))
            {
                result.Errors.push_back("Failed to convert " + pair.Zig.string() + ": " + error);
                continue;
            }
            if (sig.empty())
            {
                std::ostringstream message;
                message << "Decoded empty SIG: " << pair.Zig.string()
                        << " (input " << zigBytes.size() << " bytes)";
                result.Errors.push_back(message.str());
                continue;
            }

            std::filesystem::path sifPath = pair.Zif;
            sifPath.replace_extension(".sif");
            std::filesystem::path sigPath = pair.Zig;
            sigPath.replace_extension(".sig");
            if (!WriteFile(sifPath, sif, error))
            {
                result.Errors.push_back("Failed to write " + sifPath.string() + ": " + error);
                continue;
            }
            if (!WriteFile(sigPath, sig, error))
            {
                result.Errors.push_back("Failed to write " + sigPath.string() + ": " + error);
                continue;
            }

            result.ConvertedPairs++;
        }
    }

    return result;
}

} // namespace SeEditor::Xpac
