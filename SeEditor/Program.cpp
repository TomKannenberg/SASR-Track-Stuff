#include "Program.hpp"

#include "CharmyBee.hpp"
#include "SifParser.hpp"
#include "Frontend.hpp"
#include "NavigationLoader.hpp"
#include "LogicLoader.hpp"
#include "XpacUnpacker.hpp"
#include "Editor/Scene.hpp"
#include "Forest/ForestTypes.hpp"
#include "SlLib/Resources/Database/SlPlatform.hpp"

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <span>
#include <string>
#include <iomanip>
#include <array>
#include <filesystem>
#include <unordered_map>
#include <algorithm>
#include <optional>
#ifdef Z_SOLO
#undef Z_SOLO
#endif
#include <zlib.h>
#include <functional>
#include <cstring>
#include <cmath>

namespace SeEditor {

namespace {

constexpr int kZlibFlushNone = 0;
constexpr int kZlibOkValue = 0;
constexpr int kZlibStreamEndValue = 1;

struct ObjVertex
{
    SlLib::Math::Vector3 Pos{};
    SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
    SlLib::Math::Vector2 Uv{};
};

void DumpNavHeader(NavigationProbeInfo const& info)
{
    std::cout << "[NavCLI] Header bytes:";
    std::size_t dumpLen = std::min<std::size_t>(info.HeaderSize, 0x40);
    for (std::size_t i = 0; i < dumpLen; ++i)
    {
        if (i % 16 == 0)
            std::cout << "\n  ";
        std::cout << std::hex << std::setw(2) << std::setfill('0')
                  << static_cast<int>(info.Header[i]) << ' ';
    }
    std::cout << std::dec << std::setfill(' ') << "\n";
}

std::unordered_map<int, std::string> LoadExcelLookup(std::filesystem::path const& path)
{
    std::unordered_map<int, std::string> result;
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return result;

    std::string content((std::istreambuf_iterator<char>(file)), {});
    std::size_t i = 0;
    while (i < content.size())
    {
        if (content[i] != '"')
        {
            ++i;
            continue;
        }
        ++i;
        std::size_t keyStart = i;
        while (i < content.size() && content[i] != '"')
            ++i;
        if (i >= content.size())
            break;
        std::string keyStr = content.substr(keyStart, i - keyStart);
        ++i;
        while (i < content.size() && content[i] != ':')
            ++i;
        if (i >= content.size())
            break;
        ++i;
        while (i < content.size() && content[i] != '"')
            ++i;
        if (i >= content.size())
            break;
        ++i;
        std::size_t valueStart = i;
        while (i < content.size() && content[i] != '"')
            ++i;
        if (i >= content.size())
            break;
        std::string value = content.substr(valueStart, i - valueStart);
        ++i;

        if (keyStr.empty())
            continue;
        bool valid = true;
        std::size_t k = 0;
        if (keyStr[0] == '-')
            k = 1;
        for (; k < keyStr.size(); ++k)
        {
            if (keyStr[k] < '0' || keyStr[k] > '9')
            {
                valid = false;
                break;
            }
        }
        if (!valid)
            continue;

        try
        {
            int key = std::stoi(keyStr);
            if (!value.empty())
                result.emplace(key, value);
        }
        catch (...)
        {
        }
    }

    return result;
}

std::filesystem::path GetDefaultStuffRoot()
{
    std::filesystem::path repoRoot = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    std::filesystem::path root = repoRoot / "CppSLib_Stuff";
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
}

void DumpHashCounts(char const* label,
                    std::unordered_map<int, int> const& counts,
                    std::unordered_map<int, std::string> const& lookup)
{
    std::vector<std::pair<int, int>> items;
    items.reserve(counts.size());
    for (auto const& [key, value] : counts)
        items.emplace_back(key, value);
    std::sort(items.begin(), items.end(), [](auto const& a, auto const& b) {
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });

    std::cout << "[LogicCLI] " << label << " (" << items.size() << " types)\n";
    for (auto const& [key, value] : items)
    {
        std::cout << "  " << std::setw(6) << value << "  hash=" << key;
        auto it = lookup.find(key);
        if (it != lookup.end())
            std::cout << "  name=" << it->second;
        std::cout << "\n";
    }
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
    if (stream.empty())
        return {};

    constexpr int kZlibBufError = -5;
    z_stream inflater{};
    inflater.next_in = const_cast<std::uint8_t*>(stream.data());
    inflater.avail_in = static_cast<decltype(inflater.avail_in)>(stream.size());
    static bool loggedInitFailure = false;
    int initStatus = inflateInit(&inflater);
    if (initStatus != kZlibOkValue)
    {
        if (!loggedInitFailure)
        {
            loggedInitFailure = true;
            std::cerr << "[SIFRebuild] inflateInit failed status=" << initStatus
                      << " input=" << stream.size() << "\n";
        }
        return {};
    }

    std::vector<std::uint8_t> output;
    std::array<std::uint8_t, 16384> buffer{};
    int status = kZlibOkValue;
    static bool loggedFirstRun = false;
    while (status != kZlibStreamEndValue)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<decltype(inflater.avail_out)>(buffer.size());
        status = inflate(&inflater, kZlibFlushNone);
        if (!loggedFirstRun)
        {
            loggedFirstRun = true;
            std::cerr << "[SIFRebuild] inflate status=" << status
                      << " avail_in=" << inflater.avail_in
                      << " avail_out=" << inflater.avail_out << "\n";
        }
        if (status != kZlibOkValue && status != kZlibStreamEndValue && status != kZlibBufError)
        {
            std::cerr << "[SIFRebuild] inflate error status=" << status
                      << " input=" << stream.size() << "\n";
            inflateEnd(&inflater);
            return {};
        }
        std::size_t produced = buffer.size() - inflater.avail_out;
        if (produced > 0)
            output.insert(output.end(), buffer.begin(), buffer.begin() + produced);
        if (status == kZlibBufError && inflater.avail_in == 0 && produced == 0)
            break;
    }
    inflateEnd(&inflater);
    return output;
}

std::uint32_t ReadU32LE(std::span<const std::uint8_t> data, std::size_t offset)
{
    if (offset + 4 > data.size())
        return 0;
    return static_cast<std::uint32_t>(data[offset]) |
           (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(data[offset + 3]) << 24);
}

std::vector<std::uint8_t> DecodeZifZig(std::span<const std::uint8_t> data)
{
    if (data.empty())
        return {};

    auto stripLengthPrefix = [](std::span<const std::uint8_t> buffer) {
        if (buffer.size() < 4)
            return std::vector<std::uint8_t>(buffer.begin(), buffer.end());

        std::uint32_t expected = ReadU32LE(buffer, 0);
        std::size_t payloadSize = buffer.size() - 4;
        if (expected > 0 && expected <= payloadSize)
            return std::vector<std::uint8_t>(buffer.begin() + 4, buffer.begin() + 4 + expected);

        return std::vector<std::uint8_t>(buffer.begin(), buffer.end());
    };

    if (!LooksLikeZlib(data))
        return stripLengthPrefix(data);

    std::vector<std::uint8_t> decompressed = DecompressZlib(data);
    if (decompressed.empty())
        return {};

    return stripLengthPrefix(decompressed);
}

bool ReadFileBytes(std::filesystem::path const& path, std::vector<std::uint8_t>& out)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;
    out.assign(std::istreambuf_iterator<char>(file), {});
    return true;
}

bool WriteFileBytes(std::filesystem::path const& path, std::span<const std::uint8_t> data)
{
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return true;
}

void StripLengthPrefixIfPresent(std::vector<std::uint8_t>& data)
{
    if (data.size() < 4)
        return;

    std::uint32_t length = static_cast<std::uint32_t>(data[0]) |
                           (static_cast<std::uint32_t>(data[1]) << 8) |
                           (static_cast<std::uint32_t>(data[2]) << 16) |
                           (static_cast<std::uint32_t>(data[3]) << 24);
    if (length == data.size() - 4)
        data.erase(data.begin(), data.begin() + 4);
}

std::vector<std::uint8_t> LoadGpuDataForSif(std::filesystem::path const& sifPath,
                                            std::optional<std::filesystem::path> overridePath)
{
    std::filesystem::path gpuPath;
    if (overridePath && !overridePath->empty())
        gpuPath = *overridePath;
    else
    {
        gpuPath = sifPath;
        if (gpuPath.extension() == ".sif")
            gpuPath.replace_extension(".zig");
        else if (gpuPath.extension() == ".zif")
            gpuPath.replace_extension(".zig");
        else if (gpuPath.extension() == ".sig")
            gpuPath.replace_extension(".zig");
    }

    if (!std::filesystem::exists(gpuPath))
    {
        std::filesystem::path alt = sifPath;
        alt.replace_extension(".sig");
        if (std::filesystem::exists(alt))
            gpuPath = alt;
    }

    std::ifstream file(gpuPath, std::ios::binary);
    if (!file)
        return {};

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
    std::vector<std::uint8_t> data(buffer.begin(), buffer.end());
    if (LooksLikeZlib(data))
    {
        auto inflated = DecompressZlib(data);
        if (inflated.size() >= 4)
            data.assign(inflated.begin() + 4, inflated.end());
    }
    else
    {
        StripLengthPrefixIfPresent(data);
    }

    return data;
}

std::vector<ObjVertex> DecodeVertexStream(SeEditor::Forest::SuRenderVertexStream const& stream)
{
    using SeEditor::Forest::D3DDeclType;
    using SeEditor::Forest::D3DDeclUsage;

    std::vector<ObjVertex> verts;
    if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
        return verts;

    auto readFloat = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> float {
        if (offset + 4 > data.size())
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, data.data() + offset, sizeof(float));
        return v;
    };
    auto readU16 = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::uint16_t {
        if (offset + 2 > data.size())
            return 0;
        return static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
    };
    auto readS16 = [&](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::int16_t {
        return static_cast<std::int16_t>(readU16(data, offset));
    };

    verts.resize(static_cast<std::size_t>(stream.VertexCount));
    for (int i = 0; i < stream.VertexCount; ++i)
    {
        std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
        ObjVertex v;
        for (auto const& attr : stream.AttributeStreamsInfo)
        {
            if (attr.Stream != 0)
                continue;
            std::size_t off = base + static_cast<std::size_t>(attr.Offset);
            if (attr.Usage == D3DDeclUsage::Position)
            {
                std::size_t posOff = off;
                if (stream.StreamBias != 0)
                    posOff += static_cast<std::size_t>(stream.StreamBias);
                v.Pos = {readFloat(stream.Stream, posOff + 0),
                         readFloat(stream.Stream, posOff + 4),
                         readFloat(stream.Stream, posOff + 8)};
            }
            else if (attr.Usage == D3DDeclUsage::Normal)
            {
                if (attr.Type == D3DDeclType::Float3)
                {
                    v.Normal = {readFloat(stream.Stream, off + 0),
                                readFloat(stream.Stream, off + 4),
                                readFloat(stream.Stream, off + 8)};
                }
                else if (attr.Type == D3DDeclType::Short4N)
                {
                    v.Normal = {readS16(stream.Stream, off + 0) / 32767.0f,
                                readS16(stream.Stream, off + 2) / 32767.0f,
                                readS16(stream.Stream, off + 4) / 32767.0f};
                }
            }
            else if (attr.Usage == D3DDeclUsage::TexCoord)
            {
                if (attr.Type == D3DDeclType::Float2)
                {
                    v.Uv = {readFloat(stream.Stream, off + 0),
                            readFloat(stream.Stream, off + 4)};
                }
                else if (attr.Type == D3DDeclType::Short2N)
                {
                    v.Uv = {readS16(stream.Stream, off + 0) / 32767.0f,
                            readS16(stream.Stream, off + 2) / 32767.0f};
                }
            }
        }
        verts[static_cast<std::size_t>(i)] = v;
    }

    return verts;
}

std::vector<std::uint32_t> BuildTriangleIndices(SeEditor::Forest::SuRenderPrimitive const& primitive,
                                                std::size_t vertexLimit)
{
    std::size_t availableIndices = primitive.IndexData.size() / 2;
    std::size_t indexCount = availableIndices;
    if (primitive.NumIndices > 0)
        indexCount = std::min<std::size_t>(static_cast<std::size_t>(primitive.NumIndices), availableIndices);
    if (indexCount == 0)
        return {};

    struct IndexMode
    {
        bool Use32 = false;
        bool Swap = false;
        std::size_t Count = 0;
        std::size_t Droppable = 0;
        std::size_t Restart = 0;
    };

    auto eval16 = [&](bool swap) {
        IndexMode mode;
        mode.Use32 = false;
        mode.Swap = swap;
        mode.Count = indexCount;
        for (std::size_t i = 0; i < indexCount; ++i)
        {
            std::uint16_t a = primitive.IndexData[i * 2];
            std::uint16_t b = primitive.IndexData[i * 2 + 1];
            std::uint16_t idx = swap ? static_cast<std::uint16_t>((a << 8) | b)
                                     : static_cast<std::uint16_t>(a | (b << 8));
            if (idx == 0xFFFFu)
            {
                ++mode.Restart;
                continue;
            }
            if (static_cast<std::size_t>(idx) >= vertexLimit)
                ++mode.Droppable;
        }
        return mode;
    };

    auto eval32 = [&](bool swap) {
        IndexMode mode;
        mode.Use32 = true;
        mode.Swap = swap;
        if (primitive.IndexData.size() % 4 != 0)
            return mode;
        mode.Count = primitive.IndexData.size() / 4;
        if (primitive.NumIndices > 0)
            mode.Count = std::min<std::size_t>(mode.Count,
                static_cast<std::size_t>(primitive.NumIndices));
        for (std::size_t i = 0; i < mode.Count; ++i)
        {
            std::size_t off = i * 4;
            std::uint32_t idx = static_cast<std::uint32_t>(primitive.IndexData[off + 0] |
                (primitive.IndexData[off + 1] << 8) |
                (primitive.IndexData[off + 2] << 16) |
                (primitive.IndexData[off + 3] << 24));
            if (swap)
            {
                idx = ((idx & 0x000000FFu) << 24) |
                      ((idx & 0x0000FF00u) << 8) |
                      ((idx & 0x00FF0000u) >> 8) |
                      ((idx & 0xFF000000u) >> 24);
            }
            if (idx == 0xFFFFFFFFu)
            {
                ++mode.Restart;
                continue;
            }
            if (idx >= vertexLimit)
                ++mode.Droppable;
        }
        return mode;
    };

    IndexMode mode16le = eval16(false);
    IndexMode mode16be = eval16(true);
    IndexMode mode32le = eval32(false);
    IndexMode mode32be = eval32(true);

    IndexMode best = mode16le;
    if (mode16be.Droppable < best.Droppable)
        best = mode16be;
    if (mode32le.Count > 0 && mode32le.Droppable < best.Droppable)
        best = mode32le;
    if (mode32be.Count > 0 && mode32be.Droppable < best.Droppable)
        best = mode32be;

    bool use32Bit = best.Use32;
    bool swapIndices = best.Swap;
    std::size_t indexCount32 = best.Use32 ? best.Count : 0;
    std::size_t restart = 0;
    std::vector<std::uint32_t> rawIndices;
    rawIndices.reserve(best.Count);

    if (use32Bit)
    {
        for (std::size_t i = 0; i < indexCount32; ++i)
        {
            std::size_t off = i * 4;
            std::uint32_t idx = static_cast<std::uint32_t>(primitive.IndexData[off + 0] |
                (primitive.IndexData[off + 1] << 8) |
                (primitive.IndexData[off + 2] << 16) |
                (primitive.IndexData[off + 3] << 24));
            if (swapIndices)
            {
                idx = ((idx & 0x000000FFu) << 24) |
                      ((idx & 0x0000FF00u) << 8) |
                      ((idx & 0x00FF0000u) >> 8) |
                      ((idx & 0xFF000000u) >> 24);
            }
            if (idx == 0xFFFFFFFFu)
            {
                ++restart;
                rawIndices.push_back(idx);
                continue;
            }
            if (idx >= vertexLimit)
                continue;
            rawIndices.push_back(idx);
        }
    }
    else
    {
        for (std::size_t i = 0; i < best.Count; ++i)
        {
            std::uint16_t a = primitive.IndexData[i * 2];
            std::uint16_t b = primitive.IndexData[i * 2 + 1];
            std::uint16_t idx = swapIndices ? static_cast<std::uint16_t>((a << 8) | b)
                                            : static_cast<std::uint16_t>(a | (b << 8));
            if (idx == 0xFFFFu)
            {
                ++restart;
                rawIndices.push_back(idx);
                continue;
            }
            if (static_cast<std::size_t>(idx) >= vertexLimit)
                continue;
            rawIndices.push_back(static_cast<std::uint32_t>(idx));
        }
    }

    int primitiveType = primitive.Unknown_0x9c;
    bool isStrip = primitiveType == 5 || (primitiveType != 4 && restart > 0);
    std::vector<std::uint32_t> indices;
    if (isStrip)
    {
        indices.reserve(rawIndices.size());
        bool have0 = false;
        bool have1 = false;
        std::uint32_t i0 = 0;
        std::uint32_t i1 = 0;
        bool flip = false;
        for (std::size_t i = 0; i < rawIndices.size(); ++i)
        {
            std::uint32_t idx = rawIndices[i];
            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
            {
                have0 = false;
                have1 = false;
                flip = false;
                continue;
            }
            if (!have0)
            {
                i0 = idx;
                have0 = true;
                continue;
            }
            if (!have1)
            {
                i1 = idx;
                have1 = true;
                continue;
            }

            if (i0 != i1 && i1 != idx && i0 != idx)
            {
                if (flip)
                {
                    indices.push_back(i1);
                    indices.push_back(i0);
                    indices.push_back(idx);
                }
                else
                {
                    indices.push_back(i0);
                    indices.push_back(i1);
                    indices.push_back(idx);
                }
            }
            i0 = i1;
            i1 = idx;
            flip = !flip;
        }
    }
    else
    {
        indices.reserve(rawIndices.size());
        for (std::uint32_t idx : rawIndices)
        {
            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                continue;
            indices.push_back(idx);
        }
    }

    return indices;
}

bool TryLoadForestLibraryFromChunk(SifChunkInfo const& chunk,
                                   std::span<const std::uint8_t> gpuData,
                                   std::shared_ptr<SeEditor::Forest::ForestLibrary>& outLibrary,
                                   std::string& error)
{
    if (chunk.Data.empty())
    {
        error = "Forest chunk has no data.";
        return false;
    }

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(chunk.Relocations.size());
    for (auto offset : chunk.Relocations)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        std::span<const std::uint8_t>(chunk.Data.data(), chunk.Data.size()),
        gpuData,
        std::move(relocations));
    SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
    SlLib::Resources::Database::SlPlatform xbox360("x360", true, false, 0);
    context.Platform = chunk.BigEndian ? &xbox360 : &win32;

    auto library = std::make_shared<SeEditor::Forest::ForestLibrary>();
    try
    {
        library->Load(context);
    }
    catch (std::exception const& ex)
    {
        error = ex.what();
        return false;
    }

    outLibrary = std::move(library);
    return true;
}

std::shared_ptr<SeEditor::Forest::ForestLibrary> FindItemForestLibrary(
    std::vector<SifChunkInfo> const& chunks,
    std::span<const std::uint8_t> gpuData,
    std::string& error)
{
    for (auto const& chunk : chunks)
    {
        if (chunk.TypeValue != 0x45524F46)
            continue;

        std::shared_ptr<SeEditor::Forest::ForestLibrary> library;
        if (!TryLoadForestLibraryFromChunk(chunk, gpuData, library, error))
            continue;

        for (auto const& entry : library->Forests)
        {
            if (entry.Name == "item.Forest" && entry.Forest)
                return library;
        }
    }

    return {};
}

} // namespace

int Program::Run(int argc, char** argv)
{
    bool debugKeyInput = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--debug-keyinput")
            debugKeyInput = true;
    }

    if (argc >= 3 && std::string(argv[1]) == "--unpack-xpac")
    {
        std::filesystem::path xpacPath = argv[2];
        std::filesystem::path outputRoot = argc >= 4 ? std::filesystem::path(argv[3]) : GetDefaultStuffRoot();

        Xpac::XpacUnpackOptions options;
        options.XpacPath = xpacPath;
        options.OutputRoot = outputRoot;
        options.MappingPath = Xpac::FindDefaultMappingPath(xpacPath, outputRoot);
        options.ConvertToSifSig = true;

        Xpac::XpacUnpackResult result = Xpac::UnpackXpac(options);
        std::cout << "[XPAC] Entries=" << result.TotalEntries
                  << " ZIF=" << result.ExtractedZif
                  << " ZIG=" << result.ExtractedZig
                  << " Converted=" << result.ConvertedPairs
                  << " Skipped=" << result.SkippedEntries << std::endl;
        for (auto const& err : result.Errors)
            std::cerr << "[XPAC] " << err << std::endl;
        return result.Errors.empty() ? 0 : 1;
    }

    if (argc >= 3 && std::string(argv[1]) == "--rebuild-sif")
    {
        std::filesystem::path root = argv[2];
        if (!std::filesystem::exists(root))
        {
            std::cerr << "[SIFRebuild] Path does not exist: " << root.string() << std::endl;
            return 1;
        }

        struct PairPaths
        {
            std::filesystem::path Zif;
            std::filesystem::path Zig;
        };
        std::unordered_map<std::string, PairPaths> pairs;

        std::error_code ec;
        for (auto const& entry : std::filesystem::recursive_directory_iterator(root, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;
            std::filesystem::path path = entry.path();
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (ext != ".zif" && ext != ".zig")
                continue;

            std::filesystem::path base = path;
            base.replace_extension();
            auto& pair = pairs[base.string()];
            if (ext == ".zif")
                pair.Zif = path;
            else
                pair.Zig = path;
        }

        std::size_t converted = 0;
        std::size_t skipped = 0;
        bool printedSample = false;
        for (auto const& [base, pair] : pairs)
        {
            if (pair.Zif.empty() && pair.Zig.empty())
                continue;

            if (!pair.Zif.empty())
            {
                std::vector<std::uint8_t> data;
                if (!ReadFileBytes(pair.Zif, data))
                {
                    std::cerr << "[SIFRebuild] Failed to read " << pair.Zif.string() << std::endl;
                    ++skipped;
                }
                else
                {
                    if (!printedSample)
                    {
                        printedSample = true;
                        std::cerr << "[SIFRebuild] Sample " << pair.Zif.string()
                                  << " size=" << data.size()
                                  << " head=";
                        for (std::size_t i = 0; i < std::min<std::size_t>(8, data.size()); ++i)
                            std::cerr << std::hex << std::setw(2) << std::setfill('0')
                                      << static_cast<int>(data[i]) << ' ';
                        std::cerr << std::dec << std::setfill(' ') << "\n";
                    }
                    auto decoded = DecodeZifZig(data);
                    if (decoded.empty())
                    {
                        std::cerr << "[SIFRebuild] Empty decode for " << pair.Zif.string() << std::endl;
                        ++skipped;
                    }
                    else
                    {
                        std::filesystem::path outPath = pair.Zif;
                        outPath.replace_extension(".sif");
                        if (!WriteFileBytes(outPath, decoded))
                        {
                            std::cerr << "[SIFRebuild] Failed to write " << outPath.string() << std::endl;
                            ++skipped;
                        }
                        else
                        {
                            ++converted;
                        }
                    }
                }
            }

            if (!pair.Zig.empty())
            {
                std::vector<std::uint8_t> data;
                if (!ReadFileBytes(pair.Zig, data))
                {
                    std::cerr << "[SIFRebuild] Failed to read " << pair.Zig.string() << std::endl;
                    ++skipped;
                }
                else
                {
                    auto decoded = DecodeZifZig(data);
                    if (decoded.empty())
                    {
                        std::cerr << "[SIFRebuild] Empty decode for " << pair.Zig.string() << std::endl;
                        ++skipped;
                    }
                    else
                    {
                        std::filesystem::path outPath = pair.Zig;
                        outPath.replace_extension(".sig");
                        if (!WriteFileBytes(outPath, decoded))
                        {
                            std::cerr << "[SIFRebuild] Failed to write " << outPath.string() << std::endl;
                            ++skipped;
                        }
                        else
                        {
                            ++converted;
                        }
                    }
                }
            }
        }

        std::cout << "[SIFRebuild] Converted=" << converted << " Skipped=" << skipped << std::endl;
        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--parse-sif")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[SIFDump] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[SIFDump] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[SIFDump] Parse error: " << error << std::endl;
            return 2;
        }

        std::cout << "[SIFDump] Original size: " << data.size() << " bytes\n";
        if (parsed->WasCompressed)
            std::cout << "[SIFDump] Decompressed payload: " << parsed->DecompressedSize << " bytes\n";
        std::cout << "[SIFDump] Chunks: " << parsed->Chunks.size() << "\n";
        for (std::size_t i = 0; i < parsed->Chunks.size(); ++i)
        {
            auto const& c = parsed->Chunks[i];
            std::cout << "  [" << std::setw(2) << i << "] "
                      << c.Name << " (0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                      << c.TypeValue << std::dec << std::nouppercase << "): data=" << c.DataSize
                      << " chunk=" << c.ChunkSize << " reloc=" << c.Relocations.size() << "\n";

            if (c.TypeValue == 0x45524F46) // Forest
            {
                std::cout << "     Forest data bytes: ";
                for (std::size_t j = 0; j < std::min<std::size_t>(16, c.Data.size()); ++j)
                    std::cout << std::hex << std::setw(2) << std::setfill('0')
                              << static_cast<int>(c.Data[j]) << ' ';
                std::cout << std::dec << std::setfill(' ');
                std::cout << "\n";
                std::cout << "     Relocation offsets: ";
                for (std::size_t j = 0; j < std::min<std::size_t>(16, c.Relocations.size()); ++j)
                    std::cout << c.Relocations[j] << ' ';
                std::cout << "\n";
            }
        }
        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--parse-nav")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[NavCLI] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[NavCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[NavCLI] Parse error: " << error << std::endl;
            return 2;
        }

        SlLib::SumoTool::Siff::Navigation navigation;
        NavigationProbeInfo probe{};
        if (!LoadNavigationFromSifChunks(parsed->Chunks, navigation, probe, error))
        {
            std::cerr << "[NavCLI] Navigation parse failed: " << error << std::endl;
            return 3;
        }

        DumpNavHeader(probe);
        std::cout << "[NavCLI] chosen base=0x" << std::hex << probe.BaseOffset
                  << " version=" << std::dec << probe.Version
                  << " score=" << probe.Score << std::endl;
        std::cout << "[NavCLI] Version=" << navigation.Version
                  << " Waypoints=" << navigation.Waypoints.size()
                  << " RacingLines=" << navigation.RacingLines.size()
                  << " TrackMarkers=" << navigation.TrackMarkers.size()
                  << " SpatialGroups=" << navigation.SpatialGroups.size()
                  << " Starts=" << navigation.NavStarts.size()
                  << std::endl;

        if (!navigation.RacingLines.empty() && navigation.RacingLines[0])
            std::cout << "[NavCLI] First racing line segments="
                      << navigation.RacingLines[0]->Segments.size() << std::endl;

        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--parse-logic")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[LogicCLI] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[LogicCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[LogicCLI] Parse error: " << error << std::endl;
            return 2;
        }

        SlLib::SumoTool::Siff::LogicData logic;
        LogicProbeInfo probe{};
        if (!LoadLogicFromSifChunks(parsed->Chunks, logic, probe, error))
        {
            std::cerr << "[LogicCLI] Logic parse failed: " << error << std::endl;
            return 3;
        }

        std::cout << "[LogicCLI] chosen base=0x" << std::hex << probe.BaseOffset
                  << " version=" << std::dec << probe.Version
                  << " score=" << probe.Score << std::endl;
        std::cout << "[LogicCLI] Triggers=" << logic.Triggers.size()
                  << " Attributes=" << logic.Attributes.size()
                  << " Locators=" << logic.Locators.size()
                  << std::endl;

        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--logic-counts")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[LogicCLI] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[LogicCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[LogicCLI] Parse error: " << error << std::endl;
            return 2;
        }

        SlLib::SumoTool::Siff::LogicData logic;
        LogicProbeInfo probe{};
        if (!LoadLogicFromSifChunks(parsed->Chunks, logic, probe, error))
        {
            std::cerr << "[LogicCLI] Logic parse failed: " << error << std::endl;
            return 3;
        }

        std::filesystem::path lookupPath = std::filesystem::path("SlLib") / "Data" / "excel.lookup.json";
        auto lookup = LoadExcelLookup(lookupPath);

        std::unordered_map<int, int> byMeshTree;
        std::unordered_map<int, int> byMeshForest;
        std::unordered_map<int, int> bySetupObject;
        std::unordered_map<int, int> byAnimatedInstance;
        std::unordered_map<int, int> byLocatorName;
        std::unordered_map<int, int> byGroupName;

        for (auto const& locator : logic.Locators)
        {
            if (!locator)
                continue;
            if (locator->MeshTreeNameHash != 0)
                ++byMeshTree[locator->MeshTreeNameHash];
            if (locator->MeshForestNameHash != 0)
                ++byMeshForest[locator->MeshForestNameHash];
            if (locator->SetupObjectNameHash != 0)
                ++bySetupObject[locator->SetupObjectNameHash];
            if (locator->AnimatedInstanceNameHash != 0)
                ++byAnimatedInstance[locator->AnimatedInstanceNameHash];
            if (locator->LocatorNameHash != 0)
                ++byLocatorName[locator->LocatorNameHash];
            if (locator->GroupNameHash != 0)
                ++byGroupName[locator->GroupNameHash];
        }

        std::cout << "[LogicCLI] Locators=" << logic.Locators.size() << "\n";
        if (lookup.empty())
            std::cout << "[LogicCLI] Lookup not found at " << lookupPath.string() << " (hashes only)\n";

        DumpHashCounts("MeshTreeNameHash", byMeshTree, lookup);
        DumpHashCounts("MeshForestNameHash", byMeshForest, lookup);
        DumpHashCounts("SetupObjectNameHash", bySetupObject, lookup);
        DumpHashCounts("AnimatedInstanceNameHash", byAnimatedInstance, lookup);
        DumpHashCounts("LocatorNameHash", byLocatorName, lookup);
        DumpHashCounts("GroupNameHash", byGroupName, lookup);

        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--forest-tree-hashes")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[ForestCLI] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[ForestCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[ForestCLI] Parse error: " << error << std::endl;
            return 2;
        }

        int forestChunkIndex = 0;
        for (auto const& chunk : parsed->Chunks)
        {
            if (chunk.TypeValue != 0x45524F46)
                continue;

            std::shared_ptr<SeEditor::Forest::ForestLibrary> library;
            if (!TryLoadForestLibraryFromChunk(chunk, std::span<const std::uint8_t>(), library, error))
            {
                std::cerr << "[ForestCLI] Forest chunk " << forestChunkIndex
                          << " load failed: " << error << std::endl;
                ++forestChunkIndex;
                continue;
            }

            std::cout << "[ForestCLI] Forest chunk " << forestChunkIndex
                      << " entries=" << library->Forests.size() << "\n";
            for (std::size_t i = 0; i < library->Forests.size(); ++i)
            {
                auto const& entry = library->Forests[i];
                std::cout << "  Entry " << i << " hash=" << entry.Hash
                          << " name=" << (entry.Name.empty() ? "<empty>" : entry.Name)
                          << " trees=" << (entry.Forest ? entry.Forest->Trees.size() : 0) << "\n";
                if (!entry.Forest)
                    continue;
                if (entry.Forest->Trees.empty())
                    continue;
                auto const& tree = entry.Forest->Trees[0];
                if (tree)
                    std::cout << "    Tree0 hash=" << tree->Hash << "\n";
            }

            ++forestChunkIndex;
        }

        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--logic-itemtree-counts")
    {
        std::filesystem::path path = argv[2];
        std::cout << "[LogicCLI] Loading " << path.string() << std::endl;

        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[LogicCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[LogicCLI] Parse error: " << error << std::endl;
            return 2;
        }

        SlLib::SumoTool::Siff::LogicData logic;
        LogicProbeInfo probe{};
        if (!LoadLogicFromSifChunks(parsed->Chunks, logic, probe, error))
        {
            std::cerr << "[LogicCLI] Logic parse failed: " << error << std::endl;
            return 3;
        }

        std::shared_ptr<SeEditor::Forest::ForestLibrary> itemLibrary =
            FindItemForestLibrary(parsed->Chunks, std::span<const std::uint8_t>(), error);

        if (!itemLibrary)
        {
            std::cerr << "[LogicCLI] item.Forest not found in SIF." << std::endl;
            return 4;
        }

        std::unordered_map<int, int> meshTreeCounts;
        std::unordered_map<int, int> setupTreeCounts;
        for (auto const& locator : logic.Locators)
        {
            if (!locator)
                continue;
            if (locator->MeshTreeNameHash != 0)
                ++meshTreeCounts[locator->MeshTreeNameHash];
            if (locator->SetupObjectNameHash != 0)
                ++setupTreeCounts[locator->SetupObjectNameHash];
        }

        for (auto const& entry : itemLibrary->Forests)
        {
            if (entry.Name != "item.Forest" || !entry.Forest)
                continue;

            std::cout << "[LogicCLI] item.Forest trees=" << entry.Forest->Trees.size() << "\n";
            for (std::size_t i = 0; i < entry.Forest->Trees.size(); ++i)
            {
                auto const& tree = entry.Forest->Trees[i];
                if (!tree)
                    continue;
                int hash = tree->Hash;
                int meshCount = 0;
                int setupCount = 0;
                if (auto it = meshTreeCounts.find(hash); it != meshTreeCounts.end())
                    meshCount = it->second;
                if (auto it = setupTreeCounts.find(hash); it != setupTreeCounts.end())
                    setupCount = it->second;
                std::cout << "  Tree " << i << " hash=" << hash
                          << " meshCount=" << meshCount
                          << " setupCount=" << setupCount << "\n";
            }
            break;
        }

        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--forest-item-debug")
    {
        std::filesystem::path path = argv[2];
        std::optional<std::filesystem::path> gpuPath;
        std::optional<int> treeFilter;
        if (argc >= 4)
        {
            std::string arg3 = argv[3];
            if (!arg3.empty() && (std::isdigit(static_cast<unsigned char>(arg3[0])) || arg3[0] == '-'))
                treeFilter = std::stoi(arg3);
            else
                gpuPath = std::filesystem::path(arg3);
        }
        if (argc >= 5 && !treeFilter)
            treeFilter = std::stoi(argv[4]);

        std::cout << "[ForestCLI] Loading " << path.string() << std::endl;
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[ForestCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[ForestCLI] Parse error: " << error << std::endl;
            return 2;
        }

        auto gpuData = LoadGpuDataForSif(path, gpuPath);
        std::span<const std::uint8_t> gpuSpan;
        if (!gpuData.empty())
            gpuSpan = std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

        std::shared_ptr<SeEditor::Forest::ForestLibrary> itemLibrary =
            FindItemForestLibrary(parsed->Chunks, gpuSpan, error);
        if (!itemLibrary)
        {
            std::cerr << "[ForestCLI] item.Forest not found in SIF." << std::endl;
            return 4;
        }

        for (auto const& entry : itemLibrary->Forests)
        {
            if (entry.Name != "item.Forest" || !entry.Forest)
                continue;

            std::cout << "[ForestCLI] item.Forest trees=" << entry.Forest->Trees.size() << "\n";
            for (std::size_t t = 0; t < entry.Forest->Trees.size(); ++t)
            {
                if (treeFilter && static_cast<int>(t) != *treeFilter)
                    continue;
                auto const& tree = entry.Forest->Trees[t];
                if (!tree)
                    continue;
                std::cout << "  Tree " << t << " hash=" << tree->Hash
                          << " branches=" << tree->Branches.size() << "\n";

                for (std::size_t b = 0; b < tree->Branches.size(); ++b)
                {
                    auto const& branch = tree->Branches[b];
                    if (!branch)
                        continue;

                    auto dumpMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                        int lodIndex) {
                        if (!mesh)
                            return;
                        for (std::size_t p = 0; p < mesh->Primitives.size(); ++p)
                        {
                            auto const& prim = mesh->Primitives[p];
                            if (!prim || !prim->VertexStream)
                                continue;
                            auto const& stream = *prim->VertexStream;
                            std::cout << "    branch=" << b
                                      << " lod=" << lodIndex
                                      << " prim=" << p
                                      << " vtxCount=" << stream.VertexCount
                                      << " stride=" << stream.VertexStride
                                      << " streamBias=" << stream.StreamBias
                                      << " streamBytes=" << stream.Stream.size()
                                      << " indexBytes=" << prim->IndexData.size()
                                      << " numIndices=" << prim->NumIndices
                                      << " primType=" << prim->Unknown_0x9c
                                      << "\n";
                        }
                    };

                    if (branch->Mesh)
                        dumpMesh(branch->Mesh, -1);
                    if (branch->Lod)
                    {
                        for (std::size_t l = 0; l < branch->Lod->Thresholds.size(); ++l)
                        {
                            auto const& threshold = branch->Lod->Thresholds[l];
                            if (threshold && threshold->Mesh)
                                dumpMesh(threshold->Mesh, static_cast<int>(l));
                        }
                    }
                }
            }

            return 0;
        }

        std::cerr << "[ForestCLI] item.Forest not found in SIF." << std::endl;
        return 4;
    }

    if (argc >= 5 && std::string(argv[1]) == "--export-item-tree")
    {
        std::filesystem::path path = argv[2];
        int treeIndex = std::stoi(argv[3]);
        std::filesystem::path outputPath = argv[4];

        std::optional<std::filesystem::path> gpuPath;
        if (argc >= 6)
            gpuPath = std::filesystem::path(argv[5]);

        std::cout << "[ForestCLI] Loading " << path.string() << std::endl;
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[ForestCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[ForestCLI] Parse error: " << error << std::endl;
            return 2;
        }

        auto gpuData = LoadGpuDataForSif(path, gpuPath);
        std::span<const std::uint8_t> gpuSpan;
        if (!gpuData.empty())
            gpuSpan = std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

        std::shared_ptr<SeEditor::Forest::ForestLibrary> itemLibrary =
            FindItemForestLibrary(parsed->Chunks, gpuSpan, error);

        if (!itemLibrary)
        {
            std::cerr << "[ForestCLI] item.Forest not found in SIF." << std::endl;
            return 4;
        }

        SeEditor::Forest::SuRenderForest* forest = nullptr;
        for (auto const& entry : itemLibrary->Forests)
        {
            if (entry.Name == "item.Forest" && entry.Forest)
            {
                forest = entry.Forest.get();
                break;
            }
        }

        if (!forest)
        {
            std::cerr << "[ForestCLI] item.Forest missing data." << std::endl;
            return 5;
        }

        if (treeIndex < 0 || static_cast<std::size_t>(treeIndex) >= forest->Trees.size())
        {
            std::cerr << "[ForestCLI] Tree index out of range." << std::endl;
            return 6;
        }

        auto const& tree = forest->Trees[static_cast<std::size_t>(treeIndex)];
        if (!tree)
        {
            std::cerr << "[ForestCLI] Tree is null." << std::endl;
            return 7;
        }

        auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
            auto clamp = [](float v) { return (std::abs(v) < 1e-4f) ? 1.0f : v; };
            s.X = clamp(s.X);
            s.Y = clamp(s.Y);
            s.Z = clamp(s.Z);
            SlLib::Math::Quaternion q{r.X, r.Y, r.Z, r.W};
            SlLib::Math::Matrix4x4 rot = SlLib::Math::CreateFromQuaternion(q);
            SlLib::Math::Matrix4x4 scale{};
            scale(0, 0) = s.X;
            scale(1, 1) = s.Y;
            scale(2, 2) = s.Z;
            scale(3, 3) = 1.0f;
            SlLib::Math::Matrix4x4 local = SlLib::Math::Multiply(rot, scale);
            local(0, 3) = t.X;
            local(1, 3) = t.Y;
            local(2, 3) = t.Z;
            local(3, 3) = 1.0f;
            return local;
        };

        std::size_t branchCount = tree->Branches.size();
        std::vector<SlLib::Math::Matrix4x4> world(branchCount);
        std::vector<bool> computed(branchCount, false);
        float minScale = std::numeric_limits<float>::max();
        float maxScale = 0.0f;
        std::function<SlLib::Math::Matrix4x4(int)> computeWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
            if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                return SlLib::Math::Matrix4x4{};
            if (computed[static_cast<std::size_t>(idx)])
                return world[static_cast<std::size_t>(idx)];

            SlLib::Math::Vector4 t{};
            SlLib::Math::Vector4 r{};
            SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
            if (static_cast<std::size_t>(idx) < tree->Translations.size())
                t = tree->Translations[static_cast<std::size_t>(idx)];
            if (static_cast<std::size_t>(idx) < tree->Rotations.size())
                r = tree->Rotations[static_cast<std::size_t>(idx)];
            if (static_cast<std::size_t>(idx) < tree->Scales.size())
                s = tree->Scales[static_cast<std::size_t>(idx)];

            minScale = std::min(minScale, std::min(s.X, std::min(s.Y, s.Z)));
            maxScale = std::max(maxScale, std::max(s.X, std::max(s.Y, s.Z)));
            auto local = buildLocalMatrix(t, r, s);
            int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
            if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                world[static_cast<std::size_t>(idx)] = SlLib::Math::Multiply(computeWorld(parentIndex), local);
            else
                world[static_cast<std::size_t>(idx)] = local;

            computed[static_cast<std::size_t>(idx)] = true;
            return world[static_cast<std::size_t>(idx)];
        };

        std::vector<ObjVertex> allVertices;
        std::vector<std::array<std::uint32_t, 3>> allTriangles;

        auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                              SlLib::Math::Matrix4x4 const& worldMatrix) {
            if (!mesh)
                return;

            for (auto const& primitive : mesh->Primitives)
            {
                if (!primitive || !primitive->VertexStream)
                    continue;
                auto verts = DecodeVertexStream(*primitive->VertexStream);
                if (verts.empty())
                    continue;

                SlLib::Math::Matrix4x4 normalMatrix = worldMatrix;
                normalMatrix(0, 3) = 0.0f;
                normalMatrix(1, 3) = 0.0f;
                normalMatrix(2, 3) = 0.0f;

                for (auto& v : verts)
                {
                    SlLib::Math::Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                    auto transformed = SlLib::Math::Transform(worldMatrix, pos4);
                    v.Pos = {transformed.X, transformed.Y, transformed.Z};
                    SlLib::Math::Vector4 n4{v.Normal.X, v.Normal.Y, v.Normal.Z, 0.0f};
                    auto nT = SlLib::Math::Transform(normalMatrix, n4);
                    v.Normal = SlLib::Math::normalize({nT.X, nT.Y, nT.Z});
                }

                auto indices = BuildTriangleIndices(*primitive, verts.size());
                if (indices.empty())
                    continue;

                std::uint32_t baseIndex = static_cast<std::uint32_t>(allVertices.size());
                allVertices.insert(allVertices.end(), verts.begin(), verts.end());
                for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
                {
                    allTriangles.push_back({baseIndex + indices[i],
                                            baseIndex + indices[i + 1],
                                            baseIndex + indices[i + 2]});
                }
            }
        };

        for (std::size_t i = 0; i < branchCount; ++i)
        {
            auto worldMatrix = computeWorld(static_cast<int>(i));
            auto const& branch = tree->Branches[i];
            if (!branch)
                continue;
            if (branch->Mesh)
                appendMesh(branch->Mesh, worldMatrix);
            if (branch->Lod)
            {
                for (auto const& threshold : branch->Lod->Thresholds)
                {
                    if (threshold && threshold->Mesh)
                        appendMesh(threshold->Mesh, worldMatrix);
                }
            }
        }

        if (allVertices.empty() || allTriangles.empty())
        {
            std::cerr << "[ForestCLI] Tree has no triangles to export." << std::endl;
            return 8;
        }

        if (minScale == std::numeric_limits<float>::max())
            minScale = 0.0f;
        std::cout << "[ForestCLI] Tree scale range min=" << minScale
                  << " max=" << maxScale << "\n";

        std::ofstream output(outputPath, std::ios::binary);
        if (!output)
        {
            std::cerr << "[ForestCLI] Failed to open output file." << std::endl;
            return 9;
        }

        output << "# Exported item.Forest tree " << treeIndex << "\n";
        for (auto const& v : allVertices)
            output << "v " << v.Pos.X << " " << v.Pos.Y << " " << v.Pos.Z << "\n";
        for (auto const& v : allVertices)
            output << "vt " << v.Uv.X << " " << v.Uv.Y << "\n";
        for (auto const& v : allVertices)
            output << "vn " << v.Normal.X << " " << v.Normal.Y << " " << v.Normal.Z << "\n";
        for (auto const& tri : allTriangles)
        {
            std::uint32_t a = tri[0] + 1;
            std::uint32_t b = tri[1] + 1;
            std::uint32_t c = tri[2] + 1;
            output << "f "
                   << a << "/" << a << "/" << a << " "
                   << b << "/" << b << "/" << b << " "
                   << c << "/" << c << "/" << c << "\n";
        }

        std::cout << "[ForestCLI] Wrote " << allVertices.size()
                  << " vertices and " << allTriangles.size()
                  << " triangles to " << outputPath.string() << std::endl;
        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--item-scale-report")
    {
        std::filesystem::path path = argv[2];
        int treeA = argc >= 4 ? std::stoi(argv[3]) : 3;
        int treeB = argc >= 5 ? std::stoi(argv[4]) : 2;

        std::cout << "[ForestCLI] Loading " << path.string() << std::endl;
        std::ifstream file(path, std::ios::binary);
        if (!file)
        {
            std::cerr << "[ForestCLI] Failed to open file." << std::endl;
            return 1;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
        std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

        std::string error;
        auto parsed = ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
        if (!parsed)
        {
            std::cerr << "[ForestCLI] Parse error: " << error << std::endl;
            return 2;
        }

        auto gpuData = LoadGpuDataForSif(path, std::nullopt);
        std::span<const std::uint8_t> gpuSpan;
        if (!gpuData.empty())
            gpuSpan = std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

        std::shared_ptr<SeEditor::Forest::ForestLibrary> itemLibrary =
            FindItemForestLibrary(parsed->Chunks, gpuSpan, error);
        SeEditor::Forest::SuRenderForest* forest = nullptr;
        if (itemLibrary)
        {
            for (auto const& entry : itemLibrary->Forests)
            {
                if (entry.Name == "item.Forest" && entry.Forest)
                {
                    forest = entry.Forest.get();
                    break;
                }
            }
        }

        if (!forest)
        {
            std::cerr << "[ForestCLI] item.Forest not found in SIF." << std::endl;
            return 3;
        }

        auto reportTree = [&](int treeIndex) {
            if (treeIndex < 0 || static_cast<std::size_t>(treeIndex) >= forest->Trees.size())
            {
                std::cerr << "[ForestCLI] Tree " << treeIndex << " out of range.\n";
                return;
            }

            auto const& tree = forest->Trees[static_cast<std::size_t>(treeIndex)];
            if (!tree)
            {
                std::cerr << "[ForestCLI] Tree " << treeIndex << " is null.\n";
                return;
            }

            float minScale = std::numeric_limits<float>::max();
            float maxScale = 0.0f;
            double sumScale = 0.0;
            std::size_t count = 0;
            std::size_t tinyCount = 0;

            for (auto const& s : tree->Scales)
            {
                float localMin = std::min(s.X, std::min(s.Y, s.Z));
                float localMax = std::max(s.X, std::max(s.Y, s.Z));
                minScale = std::min(minScale, localMin);
                maxScale = std::max(maxScale, localMax);
                sumScale += (static_cast<double>(s.X) + s.Y + s.Z) / 3.0;
                ++count;
                if (localMin < 1e-6f)
                    ++tinyCount;
            }

            if (count == 0)
                minScale = 0.0f;

            std::cout << "[ForestCLI] Tree " << treeIndex
                      << " hash=" << tree->Hash
                      << " scales=" << tree->Scales.size()
                      << " min=" << minScale
                      << " max=" << maxScale
                      << " avg=" << (count ? (sumScale / count) : 0.0)
                      << " tiny<1e-6=" << tinyCount
                      << "\n";

            std::size_t sample = std::min<std::size_t>(8, tree->Scales.size());
            for (std::size_t i = 0; i < sample; ++i)
            {
                auto const& s = tree->Scales[i];
                std::cout << "  scale[" << i << "] = (" << s.X << ", " << s.Y << ", " << s.Z << ")\n";
            }
        };

        reportTree(treeA);
        reportTree(treeB);
        return 0;
    }

    std::cout << "Starting SeEditor program..." << std::endl;

    Frontend frontend(1600, 900);
    frontend.SetActiveScene(Editor::Scene());
    frontend.LoadFrontend("data/frontend/ui.pkg");

    CharmyBee editor("SeEditor", 1600, 900, debugKeyInput);
    editor.Run();
    return 0;
}

} // namespace SeEditor
