#include "XpacUnpacker.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <atomic>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <span>

#ifdef Z_SOLO
#undef Z_SOLO
#endif
#include <zlib.h>

namespace SeEditor::Xpac {

namespace {

#if defined(SEEDITOR_EMBED_MAPPING)
#include "EmbeddedMapping.hpp"
#endif

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

void WriteU32LE(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value)
{
    if (offset + 4 > data.size())
        return;
    data[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
    data[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    data[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    data[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
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

std::unordered_map<std::uint32_t, std::string> LoadMappingFromText(std::string_view text)
{
    std::unordered_map<std::uint32_t, std::string> mapping;
    std::size_t start = 0;
    while (start < text.size())
    {
        std::size_t end = text.find('\n', start);
        if (end == std::string_view::npos)
            end = text.size();
        std::string_view line = text.substr(start, end - start);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        if (!line.empty())
        {
            auto colon = line.find(':');
            auto semi = line.find(';', colon != std::string_view::npos ? colon + 1 : 0);
            if (colon != std::string_view::npos && semi != std::string_view::npos)
            {
                std::string key(line.substr(0, colon));
                std::string value(line.substr(colon + 1, semi - colon - 1));
                try
                {
                    std::uint32_t hash = static_cast<std::uint32_t>(std::stoul(key));
                    mapping.emplace(hash, value);
                }
                catch (...)
                {
                }
            }
        }
        if (end == text.size())
            break;
        start = end + 1;
    }
    return mapping;
}

std::unordered_map<std::uint32_t, std::string> LoadMapping(std::filesystem::path const& mappingPath)
{
    std::ifstream file(mappingPath);
    if (!file)
        return {};

    std::string contents((std::istreambuf_iterator<char>(file)), {});
    return LoadMappingFromText(contents);
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

bool EncodeZifZigImpl(std::span<const std::uint8_t> raw, std::vector<std::uint8_t>& out, std::string& error)
{
    if (raw.empty())
    {
        error = "Source payload is empty.";
        return false;
    }

    // ZIF/ZIG are zlib streams. If the raw data doesn't include a length prefix,
    // prepend one. This matches on-disk variants.
    std::vector<std::uint8_t> payload;
    if (raw.size() >= 4)
    {
        std::uint32_t expected = ReadU32LE(raw, 0);
        if (expected == raw.size() - 4 || expected == raw.size())
            payload.assign(raw.begin(), raw.end());
    }
    if (payload.empty())
    {
        payload.resize(raw.size() + 4);
        std::uint32_t size = static_cast<std::uint32_t>(raw.size());
        payload[0] = static_cast<std::uint8_t>(size & 0xFF);
        payload[1] = static_cast<std::uint8_t>((size >> 8) & 0xFF);
        payload[2] = static_cast<std::uint8_t>((size >> 16) & 0xFF);
        payload[3] = static_cast<std::uint8_t>((size >> 24) & 0xFF);
        std::memcpy(payload.data() + 4, raw.data(), raw.size());
    }

    z_stream stream{};
    stream.next_in = payload.data();
    stream.avail_in = static_cast<decltype(stream.avail_in)>(payload.size());

    constexpr int level = 9;
    constexpr int wbits = 15;
    constexpr int memLevel = 8;
    constexpr int strategy = Z_DEFAULT_STRATEGY;
    if (deflateInit2(&stream, level, Z_DEFLATED, wbits, memLevel, strategy) != Z_OK)
    {
        error = "Failed to init zlib deflater.";
        return false;
    }

    out.clear();
    std::vector<std::uint8_t> buffer(1 << 14);
    int status = Z_OK;
    while (status == Z_OK)
    {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<decltype(stream.avail_out)>(buffer.size());
        status = deflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END)
        {
            deflateEnd(&stream);
            error = "XPAC compression failed.";
            return false;
        }
        std::size_t have = buffer.size() - stream.avail_out;
        out.insert(out.end(), buffer.begin(), buffer.begin() + have);
    }

    deflateEnd(&stream);
    return true;
}

std::vector<std::uint8_t> CompressZlib(std::span<const std::uint8_t> input)
{
    z_stream stream{};
    stream.next_in = const_cast<std::uint8_t*>(input.data());
    stream.avail_in = static_cast<decltype(stream.avail_in)>(input.size());

    if (deflateInit(&stream, Z_BEST_COMPRESSION) != Z_OK)
        throw std::runtime_error("Failed to init zlib deflater.");

    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> buffer(1 << 14);

    int status = Z_OK;
    while (status == Z_OK)
    {
        stream.next_out = buffer.data();
        stream.avail_out = static_cast<decltype(stream.avail_out)>(buffer.size());
        status = deflate(&stream, Z_FINISH);
        if (status != Z_OK && status != Z_STREAM_END)
        {
            deflateEnd(&stream);
            throw std::runtime_error("XPAC compression failed.");
        }
        std::size_t have = buffer.size() - stream.avail_out;
        output.insert(output.end(), buffer.begin(), buffer.begin() + have);
    }

    deflateEnd(&stream);
    return output;
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

struct UnknownDecodeResult
{
    std::vector<std::uint8_t> Data;
    bool IsSif = false;
    bool Decoded = false;
};

UnknownDecodeResult DecodeUnknownPayload(std::vector<std::uint8_t> const& payload)
{
    UnknownDecodeResult result;
    if (LooksLikeSif(payload))
    {
        result.Data = payload;
        result.IsSif = true;
        return result;
    }

    std::string error;
    std::vector<std::uint8_t> decoded;
    if (DecodeZifZig(payload, decoded, error) && !decoded.empty())
    {
        result.Data = std::move(decoded);
        result.IsSif = LooksLikeSif(result.Data);
        result.Decoded = true;
        return result;
    }

    result.Data = payload;
    return result;
}

} // namespace

bool EncodeZifZig(std::span<const std::uint8_t> raw, std::vector<std::uint8_t>& out, std::string& error)
{
    return EncodeZifZigImpl(raw, out, error);
}

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
#if defined(SEEDITOR_EMBED_MAPPING)
    if (mapping.empty() && kEmbeddedMappingSize > 0)
    {
        mapping = LoadMappingFromText(
            std::string_view(reinterpret_cast<char const*>(kEmbeddedMapping), kEmbeddedMappingSize));
    }
#endif

    std::ifstream file(options.XpacPath, std::ios::binary);
    if (!file)
    {
        result.Errors.push_back("Unable to open XPAC file: " + options.XpacPath.string());
        return result;
    }

    file.seekg(0, std::ios::end);
    std::uint64_t fileSize = static_cast<std::uint64_t>(file.tellg());
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> xpacBytes(static_cast<std::size_t>(fileSize));
    if (fileSize == 0 || !file.read(reinterpret_cast<char*>(xpacBytes.data()), static_cast<std::streamsize>(xpacBytes.size())))
    {
        result.Errors.push_back("Failed to read XPAC file bytes.");
        return result;
    }

    std::array<std::uint8_t, 24> header{};
    if (xpacBytes.size() < header.size())
    {
        result.Errors.push_back("XPAC file is too small.");
        return result;
    }
    std::memcpy(header.data(), xpacBytes.data(), header.size());

    std::uint32_t totalFiles = ReadU32LE(header, 12);
    result.TotalEntries = totalFiles;

    std::size_t tableOffset = header.size();
    std::size_t tableSize = static_cast<std::size_t>(totalFiles) * 20;
    if (tableOffset + tableSize > xpacBytes.size())
    {
        result.Errors.push_back("XPAC entry table out of bounds.");
        return result;
    }

    std::vector<XpacEntry> entries;
    entries.reserve(totalFiles);
    for (std::uint32_t i = 0; i < totalFiles; ++i)
    {
        std::size_t entryOff = tableOffset + static_cast<std::size_t>(i) * 20;
        XpacEntry entry;
        entry.Hash = ReadU32LE(xpacBytes, entryOff + 0);
        entry.Offset = ReadU32LE(xpacBytes, entryOff + 4);
        entry.Size = ReadU32LE(xpacBytes, entryOff + 8);
        entry.CompressedSize = ReadU32LE(xpacBytes, entryOff + 12);
        entry.Flags = ReadU32LE(xpacBytes, entryOff + 16);
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
    std::mutex pairMutex;
    std::mutex errorMutex;
    std::mutex progressMutex;
    std::mutex resultsMutex;
    std::atomic<std::size_t> processed{0};
    std::atomic<std::size_t> skipped{0};
    std::atomic<std::size_t> extractedZif{0};
    std::atomic<std::size_t> extractedZig{0};

    struct WriteTask
    {
        std::size_t Index = 0;
        std::filesystem::path Path;
        std::vector<std::uint8_t> Data;
        bool IsMainOutput = false;
        bool IsPairCandidate = false;
        std::string PairBase;
        std::string Extension;
    };

    std::queue<WriteTask> writeQueue;
    std::mutex writeMutex;
    std::condition_variable writeCv;
    bool writerDone = false;

    struct EntryResult
    {
        bool WroteFile = false;
        bool IsSif = false;
        std::filesystem::path OutputPath;
        std::string MappedName;
    };
    std::vector<EntryResult> entryResults(entries.size());

    auto processEntry = [&](std::size_t index) {
        auto const& entry = entries[index];
        if (options.Progress)
        {
            std::lock_guard<std::mutex> lock(progressMutex);
            options.Progress(processed.load(), entries.size());
        }

        std::uint64_t endOffset = static_cast<std::uint64_t>(entry.Offset) + entry.CompressedSize;
        if (endOffset > fileSize || entry.Offset >= xpacBytes.size())
        {
            skipped.fetch_add(1);
            std::lock_guard<std::mutex> lock(errorMutex);
            result.Errors.push_back("Entry out of bounds for hash " + HashHex(entry.Hash));
            processed.fetch_add(1);
            return;
        }

        std::span<const std::uint8_t> compressedSpan(xpacBytes.data() + entry.Offset,
                                                     static_cast<std::size_t>(entry.CompressedSize));

        std::vector<std::uint8_t> payload;
        try
        {
            if (entry.CompressedSize != entry.Size)
                payload = DecompressZlib(compressedSpan, entry.Size);
            else
                payload.assign(compressedSpan.begin(), compressedSpan.end());
        }
        catch (std::exception const& ex)
        {
            skipped.fetch_add(1);
            std::lock_guard<std::mutex> lock(errorMutex);
            result.Errors.push_back("Decompression failed for hash " + HashHex(entry.Hash) + ": " + ex.what());
            processed.fetch_add(1);
            return;
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
        std::string error;
        if (!relativePath.empty())
        {
            outputPath = xpacRoot / relativePath;
            WriteTask task;
            task.Index = index;
            task.Path = outputPath;
            task.Data = std::move(payload);
            task.IsMainOutput = true;
            task.IsPairCandidate = (!extension.empty() && (extension == ".zif" || extension == ".zig"));
            task.Extension = extension;
            if (task.IsPairCandidate)
            {
                std::filesystem::path base = outputPath;
                base.replace_extension();
                task.PairBase = base.string();
            }
            {
                std::lock_guard<std::mutex> lock(writeMutex);
                writeQueue.push(std::move(task));
            }
            writeCv.notify_one();
            wroteFile = true;
        }
        else
        {
            std::filesystem::path unknownDir = xpacRoot / "unknown";
            UnknownDecodeResult unknown = DecodeUnknownPayload(payload);
            isSif = unknown.IsSif;
            std::filesystem::path binPath = unknownDir / ("hash_" + HashHex(entry.Hash) + ".bin");
            outputPath = binPath;
            {
                WriteTask task;
                task.Index = index;
                task.Path = outputPath;
                task.Data = std::move(payload);
                task.IsMainOutput = true;
                {
                    std::lock_guard<std::mutex> lock(writeMutex);
                    writeQueue.push(std::move(task));
                }
                writeCv.notify_one();
                wroteFile = true;
            }

            if (unknown.Decoded)
            {
                std::filesystem::path decodedPath =
                    unknownDir / ("hash_" + HashHex(entry.Hash) + (unknown.IsSif ? ".sif" : ".sig"));
                WriteTask task;
                task.Index = index;
                task.Path = decodedPath;
                task.Data = std::move(unknown.Data);
                {
                    std::lock_guard<std::mutex> lock(writeMutex);
                    writeQueue.push(std::move(task));
                }
                writeCv.notify_one();
            }
        }

        {
            std::lock_guard<std::mutex> lock(resultsMutex);
            entryResults[index] = {wroteFile, isSif, outputPath, mappedName};
        }
        processed.fetch_add(1);
    };

    const std::size_t workerCount = std::max<std::size_t>(1, std::thread::hardware_concurrency());
    std::atomic<std::size_t> nextIndex{0};
    std::vector<std::thread> workers;
    workers.reserve(workerCount);
    std::thread writer([&]() {
        for (;;)
        {
            WriteTask task;
            {
                std::unique_lock<std::mutex> lock(writeMutex);
                writeCv.wait(lock, [&]() { return writerDone || !writeQueue.empty(); });
                if (writeQueue.empty())
                {
                    if (writerDone)
                        break;
                    continue;
                }
                task = std::move(writeQueue.front());
                writeQueue.pop();
            }

            std::string error;
            bool ok = WriteFile(task.Path, task.Data, error);
            if (!ok)
            {
                skipped.fetch_add(1);
                std::lock_guard<std::mutex> lock(errorMutex);
                result.Errors.push_back(error);
            }

            if (task.IsMainOutput)
            {
                std::lock_guard<std::mutex> lock(resultsMutex);
                entryResults[task.Index].WroteFile = ok;
            }

            if (ok && task.IsPairCandidate)
            {
                std::lock_guard<std::mutex> lock(pairMutex);
                PairPaths& pair = pairMap[task.PairBase];
                if (task.Extension == ".zif")
                {
                    extractedZif.fetch_add(1);
                    pair.Zif = task.Path;
                }
                else if (task.Extension == ".zig")
                {
                    extractedZig.fetch_add(1);
                    pair.Zig = task.Path;
                }
            }
        }
    });
    for (std::size_t i = 0; i < workerCount; ++i)
    {
        workers.emplace_back([&]() {
            for (;;)
            {
                std::size_t index = nextIndex.fetch_add(1);
                if (index >= entries.size())
                    break;
                processEntry(index);
            }
        });
    }
    for (auto& t : workers)
        t.join();
    {
        std::lock_guard<std::mutex> lock(writeMutex);
        writerDone = true;
    }
    writeCv.notify_all();
    writer.join();

    result.SkippedEntries += skipped.load();
    result.ExtractedZif += extractedZif.load();
    result.ExtractedZig += extractedZig.load();

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        auto const& entry = entries[i];
        auto const& entryResult = entryResults[i];
        if (manifest && entryResult.WroteFile)
        {
            manifest << HashHex(entry.Hash) << ",";
            manifest << entry.Hash << ",";
            manifest << (entryResult.MappedName.empty() ? "" : entryResult.MappedName) << ",";
            manifest << entry.Size << ",";
            manifest << entry.CompressedSize << ",";
            manifest << entry.Flags << ",";
            manifest << entryResult.OutputPath.string() << ",";
            manifest << (entryResult.IsSif ? "1" : "0") << "\n";
        }
    }

    if (options.ConvertToSifSig)
    {
        std::vector<std::reference_wrapper<const PairPaths>> convertPairs;
        convertPairs.reserve(pairMap.size());
        for (auto const& [base, pair] : pairMap)
        {
            if (pair.Zif.empty() || pair.Zig.empty())
                continue;
            convertPairs.emplace_back(pair);
        }
        if (options.ProgressConvert)
            options.ProgressConvert(0, convertPairs.size());

        std::size_t convertProcessed = 0;
        for (auto const& pairRef : convertPairs)
        {
            auto const& pair = pairRef.get();
            if (options.ProgressConvert)
                options.ProgressConvert(convertProcessed, convertPairs.size());

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
            ++convertProcessed;
        }
        if (options.ProgressConvert)
            options.ProgressConvert(convertProcessed, convertPairs.size());
    }

    return result;
}

XpacRepackResult RepackXpac(XpacRepackOptions const& options)
{
    XpacRepackResult result;
    if (options.XpacPath.empty())
    {
        result.Errors.push_back("XPAC path missing.");
        return result;
    }
    if (options.InputRoot.empty())
    {
        result.Errors.push_back("Input root missing.");
        return result;
    }

    std::optional<std::filesystem::path> mappingPath = options.MappingPath;
    if (!mappingPath)
        mappingPath = FindDefaultMappingPath(options.XpacPath, options.InputRoot);
    std::unordered_map<std::uint32_t, std::string> mapping;
    if (mappingPath)
        mapping = LoadMapping(*mappingPath);
#if defined(SEEDITOR_EMBED_MAPPING)
    if (mapping.empty() && kEmbeddedMappingSize > 0)
    {
        mapping = LoadMappingFromText(
            std::string_view(reinterpret_cast<char const*>(kEmbeddedMapping), kEmbeddedMappingSize));
    }
#endif

    std::ifstream file(options.XpacPath, std::ios::binary);
    if (!file)
    {
        result.Errors.push_back("Unable to open XPAC file: " + options.XpacPath.string());
        return result;
    }

    std::array<std::uint8_t, 24> header{};
    if (!file.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size())))
    {
        result.Errors.push_back("Failed to read XPAC header.");
        return result;
    }

    std::uint32_t totalFiles = ReadU32LE(header, 12);
    result.TotalEntries = totalFiles;

    struct XpacEntryFull
    {
        XpacEntry Entry;
        bool WasCompressed = false;
    };
    std::vector<XpacEntryFull> entries;
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
        entries.push_back({entry, entry.CompressedSize != entry.Size});
    }

    std::unordered_set<std::string> selectedSet;
    selectedSet.reserve(options.SelectedSifRelativePaths.size());
    for (auto const& p : options.SelectedSifRelativePaths)
        selectedSet.insert(p.generic_string());
    bool anyReplacement = !selectedSet.empty();
    std::filesystem::path replacementRoot;
    if (options.ReplacementRoot)
        replacementRoot = *options.ReplacementRoot;

    auto readOriginalStored = [&](XpacEntryFull const& entry, std::vector<std::uint8_t>& out) -> bool {
        if (entry.Entry.CompressedSize == 0)
            return false;
        file.seekg(entry.Entry.Offset, std::ios::beg);
        out.resize(entry.Entry.CompressedSize);
        if (!file.read(reinterpret_cast<char*>(out.data()),
                       static_cast<std::streamsize>(out.size())))
            return false;
        return true;
    };

    if (!anyReplacement)
    {
        file.seekg(0, std::ios::end);
        std::streamsize fileSize = file.tellg();
        file.seekg(0, std::ios::beg);
        if (fileSize > 0)
        {
            std::vector<std::uint8_t> original(static_cast<std::size_t>(fileSize));
            if (file.read(reinterpret_cast<char*>(original.data()), fileSize))
            {
                std::ofstream out(options.OutputPath, std::ios::binary);
                if (out)
                {
                    out.write(reinterpret_cast<const char*>(original.data()),
                              static_cast<std::streamsize>(original.size()));
                    result.RepackedEntries = totalFiles;
                    return result;
                }
            }
        }
    }

    bool allReplacementsIdentical = true;
    if (anyReplacement)
    {
        for (auto const& entry : entries)
        {
            std::filesystem::path relativePath;
            auto it = mapping.find(entry.Entry.Hash);
            if (it != mapping.end())
                relativePath = BuildXpacToolRelativePath(it->second);
            if (relativePath.empty())
                continue;

            std::filesystem::path expected = relativePath;
            if (expected.extension() == ".zif")
                expected.replace_extension(".sif");
            else if (expected.extension() == ".zig")
                expected.replace_extension(".sig");
            std::string expectedName = expected.filename().generic_string();
            if (selectedSet.find(expectedName) == selectedSet.end())
                continue;

            std::filesystem::path replacement = replacementRoot.empty()
                ? (options.InputRoot / expected.filename())
                : (replacementRoot / expected.filename());
            if (!std::filesystem::exists(replacement))
            {
                allReplacementsIdentical = false;
                break;
            }

            std::vector<std::uint8_t> rawBytes;
            std::string error;
            if (!ReadFileBytes(replacement, rawBytes, error))
            {
                allReplacementsIdentical = false;
                break;
            }

            std::vector<std::uint8_t> payload;
            if (!EncodeZifZig(rawBytes, payload, error))
            {
                allReplacementsIdentical = false;
                break;
            }

            std::vector<std::uint8_t> stored = payload;
            if (entry.WasCompressed)
            {
                try
                {
                    stored = CompressZlib(payload);
                }
                catch (...)
                {
                    allReplacementsIdentical = false;
                    break;
                }
            }

            std::vector<std::uint8_t> originalStored;
            if (!readOriginalStored(entry, originalStored) ||
                originalStored.size() != stored.size() ||
                !std::equal(originalStored.begin(), originalStored.end(), stored.begin()))
            {
                allReplacementsIdentical = false;
                break;
            }
        }

        if (allReplacementsIdentical)
        {
            file.seekg(0, std::ios::end);
            std::streamsize fileSize = file.tellg();
            file.seekg(0, std::ios::beg);
            if (fileSize > 0)
            {
                std::vector<std::uint8_t> original(static_cast<std::size_t>(fileSize));
                if (file.read(reinterpret_cast<char*>(original.data()), fileSize))
                {
                    std::ofstream out(options.OutputPath, std::ios::binary);
                    if (out)
                    {
                        out.write(reinterpret_cast<const char*>(original.data()),
                                  static_cast<std::streamsize>(original.size()));
                        result.RepackedEntries = totalFiles;
                        return result;
                    }
                }
            }
        }
    }

    std::vector<std::uint8_t> output;
    std::size_t headerSize = header.size();
    std::size_t tableSize = static_cast<std::size_t>(totalFiles) * 20;
    std::size_t cursor = headerSize + tableSize;
    output.resize(cursor);

    std::uint32_t tableSizeU32 = static_cast<std::uint32_t>(tableSize);
    WriteU32LE(output, 0, ReadU32LE(header, 0));
    WriteU32LE(output, 4, ReadU32LE(header, 4));
    WriteU32LE(output, 8, tableSizeU32);
    WriteU32LE(output, 12, totalFiles);
    std::uint32_t dirTable = ReadU32LE(header, 16);
    if (dirTable == 0)
        dirTable = static_cast<std::uint32_t>(headerSize);
    WriteU32LE(output, 16, dirTable);
    WriteU32LE(output, 20, ReadU32LE(header, 20));

    auto ensureCapacity = [&](std::size_t size) {
        if (output.size() < size)
            output.resize(size);
    };

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        if (options.Progress)
            options.Progress(i, entries.size());
        auto& entry = entries[i];
        std::filesystem::path relativePath;
        auto it = mapping.find(entry.Entry.Hash);
        if (it != mapping.end())
            relativePath = BuildXpacToolRelativePath(it->second);

        std::vector<std::uint8_t> payload;
        std::string error;
        bool usedSif = false;
        auto readFromXpac = [&](std::vector<std::uint8_t>& out) -> bool {
            if (entry.Entry.CompressedSize == 0)
                return false;
            file.seekg(entry.Entry.Offset, std::ios::beg);
            std::vector<std::uint8_t> raw(entry.Entry.CompressedSize);
            if (!file.read(reinterpret_cast<char*>(raw.data()),
                           static_cast<std::streamsize>(raw.size())))
                return false;
            if (entry.WasCompressed)
            {
                try
                {
                    out = DecompressZlib(raw, entry.Entry.Size);
                }
                catch (...)
                {
                    return false;
                }
            }
            else
            {
                out = std::move(raw);
            }
            return true;
        };

        if (!relativePath.empty())
        {
            std::filesystem::path sourcePath = options.InputRoot / relativePath;
            std::filesystem::path replacement;
            if (!options.SelectedSifRelativePaths.empty())
            {
                std::filesystem::path expected = relativePath;
                if (expected.extension() == ".zif")
                    expected.replace_extension(".sif");
                else if (expected.extension() == ".zig")
                    expected.replace_extension(".sig");
                std::string expectedName = expected.filename().generic_string();
                if (selectedSet.find(expectedName) != selectedSet.end())
                {
                    if (!replacementRoot.empty())
                        replacement = replacementRoot / expected.filename();
                    else
                        replacement = options.InputRoot / expected.filename();
                }
            }

            if (!replacement.empty() && std::filesystem::exists(replacement))
            {
                std::vector<std::uint8_t> sifBytes;
                if (!ReadFileBytes(replacement, sifBytes, error))
                {
                    result.Errors.push_back("Failed to read " + replacement.string() + ": " + error);
                    return result;
                }
                if (!EncodeZifZig(sifBytes, payload, error))
                {
                    result.Errors.push_back("Failed to encode " + replacement.string() + ": " + error);
                    return result;
                }
                usedSif = true;
            }
            else
            {
                if (!ReadFileBytes(sourcePath, payload, error))
                {
                    std::filesystem::path alt = sourcePath;
                    if (alt.extension() == ".zif")
                        alt.replace_extension(".sif");
                    else if (alt.extension() == ".zig")
                        alt.replace_extension(".sig");
                    if (std::filesystem::exists(alt))
                    {
                        std::vector<std::uint8_t> rawBytes;
                        if (!ReadFileBytes(alt, rawBytes, error) ||
                            !EncodeZifZig(rawBytes, payload, error))
                        {
                            result.Errors.push_back("Failed to encode " + alt.string() + ": " + error);
                            return result;
                        }
                    }
                    else
                    {
                        if (!readFromXpac(payload))
                        {
                            result.Errors.push_back("Failed to read " + sourcePath.string() + ": " + error);
                            return result;
                        }
                    }
                }
            }
        }
        else
        {
            std::filesystem::path unknownDir = options.InputRoot / "unknown";
            std::string name = "hash_" + HashHex(entry.Entry.Hash);
            std::filesystem::path sifPath = unknownDir / (name + ".sif");
            std::filesystem::path sigPath = unknownDir / (name + ".sig");
            std::filesystem::path binPath = unknownDir / (name + ".bin");
            if (std::filesystem::exists(binPath))
            {
                if (!ReadFileBytes(binPath, payload, error))
                {
                    result.Errors.push_back("Failed to read " + binPath.string() + ": " + error);
                    return result;
                }
            }
            else if (std::filesystem::exists(sifPath))
            {
                std::vector<std::uint8_t> rawBytes;
                if (!ReadFileBytes(sifPath, rawBytes, error) ||
                    !EncodeZifZig(rawBytes, payload, error))
                {
                    result.Errors.push_back("Failed to encode " + sifPath.string() + ": " + error);
                    return result;
                }
            }
            else if (std::filesystem::exists(sigPath))
            {
                std::vector<std::uint8_t> rawBytes;
                if (!ReadFileBytes(sigPath, rawBytes, error) ||
                    !EncodeZifZig(rawBytes, payload, error))
                {
                    result.Errors.push_back("Failed to encode " + sigPath.string() + ": " + error);
                    return result;
                }
            }
            else
            {
                result.Errors.push_back("Missing payload for hash " + HashHex(entry.Entry.Hash));
                return result;
            }
        }

        std::vector<std::uint8_t> stored = payload;
        if (entry.WasCompressed)
        {
            try
            {
                stored = CompressZlib(payload);
            }
            catch (std::exception const& ex)
            {
                result.Errors.push_back("Compression failed for hash " + HashHex(entry.Entry.Hash) + ": " + ex.what());
                return result;
            }
        }

        entry.Entry.Offset = static_cast<std::uint32_t>(cursor);
        entry.Entry.Size = static_cast<std::uint32_t>(payload.size());
        entry.Entry.CompressedSize = static_cast<std::uint32_t>(stored.size());

        ensureCapacity(cursor + stored.size());
        std::memcpy(output.data() + cursor, stored.data(), stored.size());
        cursor += stored.size();
        ++result.RepackedEntries;
    }
    if (options.Progress)
        options.Progress(entries.size(), entries.size());

    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        std::size_t entryOffset = headerSize + i * 20;
        WriteU32LE(output, entryOffset + 0, entries[i].Entry.Hash);
        WriteU32LE(output, entryOffset + 4, entries[i].Entry.Offset);
        WriteU32LE(output, entryOffset + 8, entries[i].Entry.Size);
        WriteU32LE(output, entryOffset + 12, entries[i].Entry.CompressedSize);
        WriteU32LE(output, entryOffset + 16, entries[i].Entry.Flags);
    }

    std::ofstream out(options.OutputPath, std::ios::binary);
    if (!out)
    {
        result.Errors.push_back("Failed to write XPAC: " + options.OutputPath.string());
        return result;
    }
    out.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
    return result;
}

} // namespace SeEditor::Xpac
