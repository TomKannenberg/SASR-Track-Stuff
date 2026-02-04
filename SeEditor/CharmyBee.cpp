#include "CharmyBee.hpp"
#include "SifParser.hpp"

#include "Editor/Panel/IEditorPanel.hpp"
#include "Editor/SceneManager.hpp"
#include "Editor/Scene.hpp"
#include "Editor/Selection.hpp"
#include "Dialogs/TinyFileDialog.hpp"
#include "Graphics/ImGui/ImGuiController.hpp"
#include "Installers/SlModelInstaller.hpp"
#include "Installers/SlTextureInstaller.hpp"
#include "Managers/SlFile.hpp"
#include "Renderer/SlRenderer.hpp"
#include "NavigationLoader.hpp"
#include "LogicLoader.hpp"
#include "XpacUnpacker.hpp"
#include "UnityExport.hpp"
#include "Platform/Stricmp.hpp"
#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Utilities/SlUtil.hpp"
#include "Forest/ForestArchive.hpp"

#include <SlLib/Excel/ExcelData.hpp>
#include <SlLib/Enums/TriggerPhantomHashInfo.hpp>
#include <SlLib/Enums/TriggerPhantomShape.hpp>
#include <SlLib/Resources/Scene/Definitions/TriggerPhantomDefinitionNode.hpp>
#include <SlLib/Resources/Scene/SeDefinitionNode.hpp>
#include <SlLib/SumoTool/Siff/NavData/NavWaypoint.hpp>
#include <SlLib/SumoTool/Siff/Navigation.hpp>

#include <imgui.h>
#include <imgui_internal.h>

#include <limits>
#include <functional>
#include <array>
#include <cmath>
#include <unordered_set>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <chrono>
#include <fstream>
#include <functional>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <cstdint>
#include <cstring>
#include <bit>
#include <numbers>
#include <zlib.h>

namespace {

using SeEditor::SifChunkInfo;

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
    std::ofstream file(path, std::ios::binary);
    if (!file)
        return false;
    file.write(reinterpret_cast<char const*>(data.data()), static_cast<std::streamsize>(data.size()));
    return static_cast<bool>(file);
}

std::string DescribeTriggerHash(int hash)
{
    using SlLib::Enums::TriggerPhantomHashInfo;
    for (auto info : SlLib::Enums::allTriggerPhantomHashInfos())
    {
        if (static_cast<int>(info) == hash)
            return SlLib::Enums::toString(info);
    }
    return std::string("Hash ") + std::to_string(hash);
}

std::size_t AlignUp(std::size_t value, std::size_t align)
{
    if (align == 0)
        return value;
    std::size_t mask = align - 1;
    return (value + mask) & ~mask;
}

SlLib::Math::Matrix4x4 IdentityMatrix()
{
    SlLib::Math::Matrix4x4 m{};
    m(0, 0) = 1.0f;
    m(1, 1) = 1.0f;
    m(2, 2) = 1.0f;
    m(3, 3) = 1.0f;
    return m;
}

SlLib::Math::Matrix4x4 TransposeMatrix(SlLib::Math::Matrix4x4 const& in)
{
    SlLib::Math::Matrix4x4 out{};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            out(r, c) = in(c, r);
    return out;
}

float IdentityError(SlLib::Math::Matrix4x4 const& m)
{
    float err = 0.0f;
    for (int r = 0; r < 4; ++r)
    {
        for (int c = 0; c < 4; ++c)
        {
            float target = (r == c) ? 1.0f : 0.0f;
            err += std::fabs(m(r, c) - target);
        }
    }
    return err;
}

void WriteInt32LE(std::vector<std::uint8_t>& out, std::size_t offset, std::int32_t value)
{
    if (offset + 4 > out.size())
        return;
    out[offset + 0] = static_cast<std::uint8_t>(value & 0xFF);
    out[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
    out[offset + 2] = static_cast<std::uint8_t>((value >> 16) & 0xFF);
    out[offset + 3] = static_cast<std::uint8_t>((value >> 24) & 0xFF);
}

void WriteFloatLE(std::vector<std::uint8_t>& out, std::size_t offset, float value)
{
    std::uint32_t raw = 0;
    static_assert(sizeof(raw) == sizeof(value));
    std::memcpy(&raw, &value, sizeof(raw));
    WriteInt32LE(out, offset, static_cast<std::int32_t>(raw));
}

std::int32_t ReadInt32LE(const std::uint8_t* ptr)
{
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(ptr[0]) |
                                     (static_cast<std::uint32_t>(ptr[1]) << 8) |
                                     (static_cast<std::uint32_t>(ptr[2]) << 16) |
                                     (static_cast<std::uint32_t>(ptr[3]) << 24));
}

std::string DescribeGlfwKey(int key)
{
    if (auto name = glfwGetKeyName(key, 0))
        return std::string(name);

    switch (key)
    {
    case GLFW_KEY_SPACE: return "Space";
    case GLFW_KEY_ESCAPE: return "Escape";
    case GLFW_KEY_ENTER: return "Enter";
    case GLFW_KEY_TAB: return "Tab";
    case GLFW_KEY_BACKSPACE: return "Backspace";
    case GLFW_KEY_INSERT: return "Insert";
    case GLFW_KEY_DELETE: return "Delete";
    case GLFW_KEY_RIGHT: return "Right";
    case GLFW_KEY_LEFT: return "Left";
    case GLFW_KEY_DOWN: return "Down";
    case GLFW_KEY_UP: return "Up";
    case GLFW_KEY_PAGE_UP: return "PageUp";
    case GLFW_KEY_PAGE_DOWN: return "PageDown";
    case GLFW_KEY_HOME: return "Home";
    case GLFW_KEY_END: return "End";
    case GLFW_KEY_CAPS_LOCK: return "CapsLock";
    case GLFW_KEY_SCROLL_LOCK: return "ScrollLock";
    case GLFW_KEY_NUM_LOCK: return "NumLock";
    case GLFW_KEY_PRINT_SCREEN: return "PrintScreen";
    case GLFW_KEY_PAUSE: return "Pause";
    case GLFW_KEY_F1: return "F1";
    case GLFW_KEY_F2: return "F2";
    case GLFW_KEY_F3: return "F3";
    case GLFW_KEY_F4: return "F4";
    case GLFW_KEY_F5: return "F5";
    case GLFW_KEY_F6: return "F6";
    case GLFW_KEY_F7: return "F7";
    case GLFW_KEY_F8: return "F8";
    case GLFW_KEY_F9: return "F9";
    case GLFW_KEY_F10: return "F10";
    case GLFW_KEY_F11: return "F11";
    case GLFW_KEY_F12: return "F12";
    default:
        return "Key-" + std::to_string(key);
    }
}

void ReportSifError(std::string const& message)
{
    std::cerr << "[CharmyBee][SIF] " << message << std::endl;
}

std::optional<std::pair<int, int>> ParseWaypointRoute(std::string const& name)
{
    constexpr std::string_view prefix = "waypoint_";
    if (name.rfind(prefix, 0) != 0)
        return std::nullopt;

    std::string rest = name.substr(prefix.size());
    auto delim = rest.find('_');
    if (delim == std::string::npos)
        return std::nullopt;

    try
    {
        int routeId = std::stoi(rest.substr(0, delim));
        int pointId = std::stoi(rest.substr(delim + 1));
        return std::make_pair(routeId, pointId);
    }
    catch (...)
    {
        return std::nullopt;
    }
}


std::vector<std::string> FormatHexDump(std::vector<std::uint8_t> const& data)
{
    constexpr char hexDigits[] = "0123456789ABCDEF";
    std::vector<std::string> lines;
    size_t totalLines = (data.size() + 15) / 16;
    lines.reserve(std::max<size_t>(1, totalLines));

    for (size_t offset = 0; offset < data.size(); offset += 16)
    {
        char header[32];
        std::snprintf(header, sizeof(header), "%08llX: ", static_cast<unsigned long long>(offset));
        std::string line = header;

        for (size_t column = 0; column < 16; ++column)
        {
            if (offset + column < data.size())
            {
                auto byte = data[offset + column];
                line.push_back(hexDigits[(byte >> 4) & 0xF]);
                line.push_back(hexDigits[byte & 0xF]);
                line.push_back(' ');
            }
            else
            {
                line.append("   ");
            }
        }

        line.append(" | ");
        for (size_t column = 0; column < 16; ++column)
        {
            if (offset + column < data.size())
            {
                auto byte = data[offset + column];
                line.push_back(std::isprint(byte) ? static_cast<char>(byte) : '.');
            }
            else
            {
                line.push_back(' ');
            }
        }

        lines.push_back(std::move(line));
    }

    if (lines.empty())
        lines.push_back("File is empty.");

    return lines;
}

constexpr std::uint32_t MakeTypeCode(char a, char b, char c, char d)
{
    return static_cast<std::uint32_t>(static_cast<unsigned char>(a)) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(b)) << 8) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(c)) << 16) |
           (static_cast<std::uint32_t>(static_cast<unsigned char>(d)) << 24);
}

constexpr int kZlibFlushNone = 0;
constexpr int kZlibOkValue = 0;
constexpr int kZlibStreamEndValue = 1;

enum class SifResourceType : std::uint32_t
{
    Info = 0x4F464E49,
    TexturePack = 0x58455450,
    KeyFrameLibrary = 0x4D52464B,
    ObjectDefLibrary = 0x534A424F,
    SceneLibrary = 0x454E4353,
    FontPack = 0x544E4F46,
    TextPack = 0x54584554,
    Relocations = 0x4F4C4552,
    Forest = 0x45524F46,
    Navigation = 0x4B415254,
    Trail = 0x4C415254,
    Logic = 0x43474F4C,
    VisData = 0x34445650,
    ShData = 0x31304853,
    LensFlare2 = 0x3220464C,
    LensFlare1 = 0x3120464C,
    Collision = 0x494C4F43,
};

std::string ResourceTypeName(std::uint32_t value)
{
    switch (static_cast<SifResourceType>(value))
    {
    case SifResourceType::Info:
        return "Info";
    case SifResourceType::TexturePack:
        return "TexturePack";
    case SifResourceType::KeyFrameLibrary:
        return "KeyFrameLibrary";
    case SifResourceType::ObjectDefLibrary:
        return "ObjectDefLibrary";
    case SifResourceType::SceneLibrary:
        return "SceneLibrary";
    case SifResourceType::FontPack:
        return "FontPack";
    case SifResourceType::TextPack:
        return "TextPack";
    case SifResourceType::Forest:
        return "Forest";
    case SifResourceType::Navigation:
        return "Navigation";
    case SifResourceType::Trail:
        return "Trail";
    case SifResourceType::Logic:
        return "Logic";
    case SifResourceType::VisData:
        return "VisData";
    case SifResourceType::ShData:
        return "ShData";
    case SifResourceType::LensFlare2:
        return "LensFlare2";
    case SifResourceType::LensFlare1:
        return "LensFlare1";
    case SifResourceType::Collision:
        return "Collision";
    default:
        return "Unknown";
    }
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

std::uint16_t ReadUint16(const std::uint8_t* ptr)
{
    return static_cast<std::uint16_t>(ptr[0]) |
           (static_cast<std::uint16_t>(ptr[1]) << 8);
}

float HalfToFloat(std::uint16_t value)
{
    std::uint16_t sign = (value >> 15) & 1;
    std::uint16_t exp = (value >> 10) & 0x1F;
    std::uint16_t mant = value & 0x3FF;

    if (exp == 0)
    {
        if (mant == 0)
            return sign ? -0.0f : 0.0f;
        float m = mant / 1024.0f;
        float val = std::ldexp(m, -14);
        return sign ? -val : val;
    }
    if (exp == 31)
        return mant ? std::numeric_limits<float>::quiet_NaN()
                    : (sign ? -std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::infinity());

    float m = 1.0f + mant / 1024.0f;
    float val = std::ldexp(m, exp - 15);
    return sign ? -val : val;
}

float ReadFloatLE(const std::uint8_t* ptr)
{
    std::uint32_t v = ReadUint32(ptr);
    float out;
    std::memcpy(&out, &v, sizeof(float));
    return out;
}

float ReadFloatBE(const std::uint8_t* ptr)
{
    std::uint32_t v = ReadUint32BE(ptr);
    float out;
    std::memcpy(&out, &v, sizeof(float));
    return out;
}

bool ParseCollisionMeshChunk(SifChunkInfo const& chunk,
                             std::vector<SlLib::Math::Vector3>& vertices,
                             std::vector<std::array<int, 3>>& triangles,
                             std::string& error)
{
    using SlLib::Math::Vector3;
    auto const& data = chunk.Data;
    bool be = chunk.BigEndian;
    auto read32 = [&](std::size_t off) -> std::uint32_t {
        if (off + 4 > data.size())
            return 0;
        return be ? ReadUint32BE(data.data() + off) : ReadUint32(data.data() + off);
    };
    auto read16 = [&](std::size_t off) -> std::uint16_t {
        if (off + 2 > data.size())
            return 0;
        return be ? static_cast<std::uint16_t>((data[off] << 8) | data[off + 1])
                  : static_cast<std::uint16_t>(data[off] | (data[off + 1] << 8));
    };
    auto readFloat = [&](std::size_t off) -> float {
        if (off + 4 > data.size())
            return 0.0f;
        return be ? ReadFloatBE(data.data() + off) : ReadFloatLE(data.data() + off);
    };

    if (data.size() < 0x48)
    {
        error = "Collision chunk too small.";
        return false;
    }

    std::uint32_t numVertices = read32(0x8);
    std::uint32_t numTriangles = read32(0xC);

    std::uint32_t verticesPtr = read32(0x30);
    std::uint32_t trianglesPtr = read32(0x34);

    if (verticesPtr == 0 || trianglesPtr == 0)
    {
        error = "Collision chunk missing vertex/triangle pointers.";
        return false;
    }

    if (verticesPtr + numVertices * 0x10 > data.size())
    {
        error = "Collision vertices outside chunk bounds.";
        return false;
    }
    if (trianglesPtr + numTriangles * 0x0C > data.size())
    {
        error = "Collision triangles outside chunk bounds.";
        return false;
    }

    vertices.clear();
    vertices.reserve(numVertices);
    for (std::size_t i = 0; i < numVertices; ++i)
    {
        std::size_t off = verticesPtr + i * 0x10;
        Vector3 v{
            readFloat(off + 0),
            readFloat(off + 4),
            readFloat(off + 8)
        };
        vertices.push_back(v);
    }

    triangles.clear();
    triangles.reserve(numTriangles);
    for (std::size_t i = 0; i < numTriangles; ++i)
    {
        std::size_t off = trianglesPtr + i * 0x0C;
        std::uint16_t v0 = read16(off + 0);
        std::uint16_t v1 = read16(off + 2);
        std::uint16_t v2 = read16(off + 4);
        if (v0 >= vertices.size() || v1 >= vertices.size() || v2 >= vertices.size())
            continue;
        triangles.push_back({static_cast<int>(v0), static_cast<int>(v1), static_cast<int>(v2)});
    }

    return true;
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

std::uint32_t ReadUint32LE(const std::uint8_t* ptr)
{
    return static_cast<std::uint32_t>(ptr[0]) |
           (static_cast<std::uint32_t>(ptr[1]) << 8) |
           (static_cast<std::uint32_t>(ptr[2]) << 16) |
           (static_cast<std::uint32_t>(ptr[3]) << 24);
}

void StripLengthPrefixIfPresent(std::vector<std::uint8_t>& data)
{
    if (data.size() < 4)
        return;

    std::size_t size = data.size();
    std::uint32_t le = ReadUint32LE(data.data());
    std::uint32_t be = ReadUint32BE(data.data());
    if (le == size - 4 || le == size || be == size - 4 || be == size)
        data.erase(data.begin(), data.begin() + 4);
}

struct SifParseResult
{
    bool WasCompressed = false;
    std::size_t DecompressedSize = 0;
    std::vector<SifChunkInfo> Chunks;
};

std::string FormatRelocationList(std::vector<std::uint32_t> const& relocations)
{
    if (relocations.empty())
        return {};

    std::ostringstream builder;
    builder << "  Relocations:";
    std::size_t limit = std::min<std::size_t>(relocations.size(), 6);
    for (std::size_t i = 0; i < limit; ++i)
    {
        builder << " 0x" << std::hex << std::uppercase << std::setw(8) << std::setfill('0')
                << relocations[i];
    }

    if (relocations.size() > limit)
        builder << " ...";

    return builder.str();
}

std::string QuoteArgument(std::string const& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char ch : value)
    {
        if (ch == '"')
            escaped.append("\\\"");
        else
            escaped.push_back(ch);
    }
    escaped.push_back('"');
    return escaped;
}

std::uint64_t MakeForestTreeKey(int forestHash, int treeHash)
{
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(forestHash)) << 32) |
           static_cast<std::uint32_t>(treeHash);
}

bool TryParseForestArchive(std::span<const std::uint8_t> input,
                           std::vector<std::uint8_t>& cpuData,
                           std::vector<std::uint32_t>& relocations,
                           std::vector<std::uint8_t>& gpuData,
                           bool& bigEndian)
{
    using SeEditor::Forest::kForestArchiveFlagBigEndian;
    using SeEditor::Forest::kForestArchiveMagic;

    if (input.size() < 20)
        return false;

    auto readU32 = [&](std::size_t offset) -> std::uint32_t {
        if (offset + 4 > input.size())
            return 0;
        return static_cast<std::uint32_t>(input[offset]) |
               (static_cast<std::uint32_t>(input[offset + 1]) << 8) |
               (static_cast<std::uint32_t>(input[offset + 2]) << 16) |
               (static_cast<std::uint32_t>(input[offset + 3]) << 24);
    };

    std::uint32_t magic = readU32(0);
    std::uint32_t version = readU32(4);
    if (magic != kForestArchiveMagic)
        return false;

    std::size_t headerSize = 0;
    std::uint32_t flags = 0;
    std::size_t chunkSize = 0;
    std::size_t relocCount = 0;
    std::size_t gpuSize = 0;

    if (version == 1)
    {
        headerSize = 20;
        chunkSize = readU32(8);
        relocCount = readU32(12);
        gpuSize = readU32(16);
    }
    else if (version == 2)
    {
        headerSize = 24;
        flags = readU32(8);
        chunkSize = readU32(12);
        relocCount = readU32(16);
        gpuSize = readU32(20);
    }
    else
    {
        return false;
    }

    bigEndian = (flags & kForestArchiveFlagBigEndian) != 0;

    constexpr std::size_t kUint32Size = sizeof(std::uint32_t);
    auto safeAdd = [](std::size_t a, std::size_t b) -> std::optional<std::size_t> {
        if (b > std::numeric_limits<std::size_t>::max() - a)
            return std::nullopt;
        return a + b;
    };

    auto total = safeAdd(headerSize, chunkSize);
    if (!total)
        return false;
    auto withRelocs = safeAdd(*total, relocCount * kUint32Size);
    if (!withRelocs)
        return false;
    auto withGpu = safeAdd(*withRelocs, gpuSize);
    if (!withGpu)
        return false;

    if (input.size() < *withGpu)
        return false;

    cpuData.assign(input.begin() + headerSize, input.begin() + headerSize + chunkSize);
    relocations.clear();
    relocations.reserve(relocCount);
    auto const* relocBase = reinterpret_cast<const std::uint32_t*>(input.data() + headerSize + chunkSize);
    for (std::size_t i = 0; i < relocCount; ++i)
        relocations.push_back(relocBase[i]);

    auto gpuStart = input.data() + headerSize + chunkSize + relocCount * kUint32Size;
    gpuData.assign(gpuStart, gpuStart + gpuSize);
    return true;
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

      std::span<const std::uint8_t> forestData(chunk.Data.data(), chunk.Data.size());

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(chunk.Relocations.size());
    for (auto offset : chunk.Relocations)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        forestData,
        gpuData,
        std::move(relocations));
    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_ps3("ps3", true, false, 0);
    context.Platform = chunk.BigEndian ? &s_ps3 : &s_win32;

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

void BuildForestTreeMeshMaps(SeEditor::Forest::ForestLibrary const& library,
                             bool isBigEndian,
                             std::unordered_map<std::uint64_t,
                                                std::shared_ptr<std::vector<SeEditor::Renderer::SlRenderer::ForestCpuMesh>>>&
                                 byForestTree,
                             std::unordered_map<int,
                                                std::shared_ptr<std::vector<SeEditor::Renderer::SlRenderer::ForestCpuMesh>>>&
                                 byTreeHash)
{
    using SeEditor::Forest::D3DDeclType;
    using SeEditor::Forest::D3DDeclUsage;
    using SeEditor::Renderer::SlRenderer;

    byForestTree.clear();
    byTreeHash.clear();

    struct ForestVertex
    {
        SlLib::Math::Vector3 Pos{};
        SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
        SlLib::Math::Vector2 Uv{};
        std::array<float, 4> Weights{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> Indices{0.0f, 0.0f, 0.0f, 0.0f};
    };

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
    auto readU8 = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::uint8_t {
        if (offset >= data.size())
            return 0;
        return data[offset];
    };

    auto decodeVertex = [&](SeEditor::Forest::SuRenderVertexStream const& stream) {
        std::vector<ForestVertex> verts;
        if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
            return verts;

        verts.resize(static_cast<std::size_t>(stream.VertexCount));
        for (int i = 0; i < stream.VertexCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
            ForestVertex v;
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
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
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
    };

    auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
        auto clamp = [](float v) { return (std::abs(v) < 1e-4f) ? 1.0f : v; };
        auto safe = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
        t.X = safe(t.X, 0.0f);
        t.Y = safe(t.Y, 0.0f);
        t.Z = safe(t.Z, 0.0f);
        s.X = clamp(s.X);
        s.Y = clamp(s.Y);
        s.Z = clamp(s.Z);
        s.X = safe(s.X, 1.0f);
        s.Y = safe(s.Y, 1.0f);
        s.Z = safe(s.Z, 1.0f);
        SlLib::Math::Quaternion q{r.X, r.Y, r.Z, r.W};
        float qLen = std::sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
        if (!std::isfinite(qLen) || qLen < 1e-6f)
        {
            q = {0.0f, 0.0f, 0.0f, 1.0f};
        }
        else
        {
            float invLen = 1.0f / qLen;
            q = q * invLen;
        }
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

    std::size_t debugDroppedLogged = 0;

    for (auto const& forestEntry : library.Forests)
    {
        if (!forestEntry.Forest)
            continue;

        int forestHash = forestEntry.Hash;
        int forestNameHash = 0;
        if (!forestEntry.Name.empty())
            forestNameHash = SlLib::Utilities::HashString(forestEntry.Name);

        auto const& trees = forestEntry.Forest->Trees;
        for (auto const& tree : trees)
        {
            if (!tree)
                continue;

            std::vector<SlRenderer::ForestCpuMesh> treeMeshes;

            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);

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

                auto local = buildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        SlLib::Math::Multiply(computeWorld(parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                  SlLib::Math::Matrix4x4 const& worldMatrix) {
                if (!mesh)
                    return;

                for (std::size_t primIndex = 0; primIndex < mesh->Primitives.size(); ++primIndex)
                {
                    auto const& primitive = mesh->Primitives[primIndex];
                    if (!primitive || !primitive->VertexStream)
                        continue;

                    auto verts = decodeVertex(*primitive->VertexStream);
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

                    SlRenderer::ForestCpuMesh cpu;
                    cpu.Model = IdentityMatrix();
                    cpu.Skinned = false;
                    cpu.Vertices.reserve(verts.size() * 16);
                    for (auto const& v : verts)
                    {
                        cpu.Vertices.push_back(v.Pos.X);
                        cpu.Vertices.push_back(v.Pos.Y);
                        cpu.Vertices.push_back(v.Pos.Z);
                        cpu.Vertices.push_back(v.Normal.X);
                        cpu.Vertices.push_back(v.Normal.Y);
                        cpu.Vertices.push_back(v.Normal.Z);
                        cpu.Vertices.push_back(v.Uv.X);
                        cpu.Vertices.push_back(v.Uv.Y);
                        cpu.Vertices.push_back(1.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                        cpu.Vertices.push_back(0.0f);
                    }

                    std::size_t availableIndices = primitive->IndexData.size() / 2;
                    std::size_t indexCount = availableIndices;
                    if (primitive->NumIndices > 0)
                        indexCount = std::min(static_cast<std::size_t>(primitive->NumIndices), availableIndices);

                    if (indexCount == 0)
                        continue;

                    std::size_t vertexLimit = verts.size();
                    struct IndexMode
                    {
                        bool Use32 = false;
                        bool Swap = false;
                        std::size_t Count = 0;
                        std::size_t Droppable = 0;
                        std::size_t Restart = 0;
                        std::uint32_t MaxIndex = 0;
                    };

                    auto eval16 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = false;
                        mode.Swap = swap;
                        mode.Count = indexCount;
                        for (std::size_t i = 0; i < indexCount; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swap ? static_cast<std::uint16_t>((a << 8) | b)
                                                     : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++mode.Restart;
                                continue;
                            }
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                                ++mode.Droppable;
                        }
                        return mode;
                    };

                    auto eval32 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = true;
                        mode.Swap = swap;
                        if (primitive->IndexData.size() % 4 != 0)
                            return mode;
                        mode.Count = primitive->IndexData.size() / 4;
                        if (primitive->NumIndices > 0)
                            mode.Count = std::min<std::size_t>(mode.Count,
                                static_cast<std::size_t>(primitive->NumIndices));
                        for (std::size_t i = 0; i < mode.Count; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
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
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
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
                    std::size_t droppable = 0;
                    std::size_t restart = 0;
                    std::vector<std::uint32_t> rawIndices;
                    rawIndices.reserve(best.Count);

                    if (use32Bit)
                    {
                        for (std::size_t i = 0; i < indexCount32; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
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
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(idx);
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < best.Count; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swapIndices ? static_cast<std::uint16_t>((a << 8) | b)
                                                            : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++restart;
                                rawIndices.push_back(idx);
                                continue;
                            }
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(static_cast<std::uint32_t>(idx));
                        }
                    }

                    int primitiveType = primitive->Unknown_0x9c;
                    bool isStrip = primitiveType == 5 || (primitiveType != 4 && restart > 0);
                    if (isStrip)
                    {
                        cpu.Indices.reserve(rawIndices.size());
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
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(idx);
                                }
                                else
                                {
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(idx);
                                }
                            }
                            i0 = i1;
                            i1 = idx;
                            flip = !flip;
                        }
                    }
                    else
                    {
                        cpu.Indices.reserve(rawIndices.size());
                        for (std::uint32_t idx : rawIndices)
                        {
                            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                                continue;
                            cpu.Indices.push_back(idx);
                        }
                    }

                    if (droppable > 0 || restart > 0)
                    {
                        if (debugDroppedLogged < 2 && primitive->VertexStream)
                        {
                            ++debugDroppedLogged;
                            std::cerr << "[Forest] Dropped " << droppable << " indices for item mesh ("
                                      << vertexLimit << " verts), restart=" << restart
                                      << " endian=" << (isBigEndian ? "BE" : "LE")
                                      << " primType=" << primitiveType << '\n';
                        }
                    }

                    if (cpu.Indices.empty())
                        continue;

                    if (primitive->Material && !primitive->Material->Textures.empty())
                        cpu.Texture = primitive->Material->Textures[0]->TextureResource;
                    treeMeshes.push_back(std::move(cpu));
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

            if (treeMeshes.empty())
                continue;

            auto meshPtr = std::make_shared<std::vector<SlRenderer::ForestCpuMesh>>(std::move(treeMeshes));
            int treeHash = tree->Hash;
            auto insertTree = [&](int hash) {
                auto key = MakeForestTreeKey(hash, treeHash);
                if (byForestTree.find(key) == byForestTree.end())
                    byForestTree.emplace(key, meshPtr);
            };

            if (treeHash != 0 && byTreeHash.find(treeHash) == byTreeHash.end())
                byTreeHash.emplace(treeHash, meshPtr);

            if (forestHash != 0)
                insertTree(forestHash);
            if (forestNameHash != 0 && forestNameHash != forestHash)
                insertTree(forestNameHash);
        }
    }
}

} // namespace

namespace SeEditor {

void* CharmyBee::SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler* handler, const char* name)
{
    if (!handler || !name || std::strcmp(name, "Data") != 0)
        return nullptr;
    return handler->UserData;
}

void CharmyBee::SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler* handler, void* entry, const char* line)
{
    auto* self = static_cast<CharmyBee*>(entry ? entry : (handler ? handler->UserData : nullptr));
    if (!self || !line)
        return;
    constexpr char key[] = "StuffRoot=";
    if (std::strncmp(line, key, sizeof(key) - 1) == 0)
    {
        const char* value = line + sizeof(key) - 1;
        if (!value || *value == '\0')
        {
            self->_stuffRootOverride.reset();
            return;
        }
        self->_stuffRootOverride = std::filesystem::path(value);
    }
}

void CharmyBee::SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf)
{
    auto* self = static_cast<CharmyBee*>(handler ? handler->UserData : nullptr);
    if (!self || !buf)
        return;
    buf->appendf("[%s][Data]\n", handler->TypeName);
    if (self->_stuffRootOverride && !self->_stuffRootOverride->empty())
        buf->appendf("StuffRoot=%s\n", self->_stuffRootOverride->string().c_str());
    buf->append("\n");
}

CharmyBee::CharmyBee(std::string title, int width, int height, bool debugKeyInput)
    : _title(std::move(title))
    , _width(width)
    , _height(height)
    , _debugKeyInput(debugKeyInput)
{
    ResetAssetTree();
}

CharmyBee::~CharmyBee()
{
    if (_xpacWorker && _xpacWorker->joinable())
        _xpacWorker->join();
}

CharmyBee::TreeNode::TreeNode(std::string name, bool isFolder)
    : Name(std::move(name))
    , IsFolder(isFolder)
{}

void CharmyBee::OnLoad()
{
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowMenuButtonPosition = ImGuiDir_None;
    style.TabRounding = 1.0f;
    style.FrameRounding = 1.0f;
    style.ScrollbarRounding = 1.0f;
    style.WindowRounding = 0.0f;

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigDragClickToInputText = true;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    ImGuiContext* context = ImGui::GetCurrentContext();
    if (context)
    {
        ImGuiSettingsHandler handler;
        handler.TypeName = "CppSLib";
        handler.TypeHash = ImHashStr("CppSLib");
        handler.UserData = this;
        handler.ReadOpenFn = &CharmyBee::SettingsReadOpen;
        handler.ReadLineFn = &CharmyBee::SettingsReadLine;
        handler.WriteAllFn = &CharmyBee::SettingsWriteAll;

        bool alreadyRegistered = false;
        for (auto const& existing : context->SettingsHandlers)
        {
            if (existing.TypeHash == handler.TypeHash)
            {
                alreadyRegistered = true;
                break;
            }
        }
        if (!alreadyRegistered)
            ImGui::AddSettingsHandler(&handler);
    }

    _renderer.Initialize();
}

void CharmyBee::SetupNavigationRendering(SlLib::SumoTool::Siff::Navigation* navigation)
{
    _navigation = navigation;
    if (_navigation == nullptr)
        return;

    _navigationTool = std::make_unique<Editor::Tools::NavigationTool>(_navigation);

    struct RouteInfo
    {
        int Id;
        std::vector<std::pair<int, SlLib::SumoTool::Siff::NavData::NavWaypoint*>> Waypoints;
    };

    std::vector<RouteInfo> routeInfos;
    for (auto const& waypoint : _navigation->Waypoints)
    {
        if (waypoint == nullptr)
            continue;

        if (auto parsed = ParseWaypointRoute(waypoint->Name))
        {
            auto it = std::find_if(routeInfos.begin(), routeInfos.end(),
                [parsed](RouteInfo const& info) { return info.Id == parsed->first; });
            if (it == routeInfos.end())
            {
                routeInfos.push_back(RouteInfo{parsed->first, {}});
                it = std::prev(routeInfos.end());
            }

            it->Waypoints.emplace_back(parsed->second, waypoint.get());
        }
    }

    std::sort(routeInfos.begin(), routeInfos.end(), [](RouteInfo const& a, RouteInfo const& b) {
        return a.Id < b.Id;
    });

    _routes.clear();
    for (auto& info : routeInfos)
    {
        std::sort(info.Waypoints.begin(), info.Waypoints.end(),
            [](auto const& lhs, auto const& rhs) { return lhs.first < rhs.first; });

        Editor::Tools::NavTool::NavRoute route(info.Id);
        for (auto const& entry : info.Waypoints)
            route.Waypoints.push_back(entry.second);

        _routes.push_back(std::move(route));
    }

    if (!_routes.empty())
    {
        _selectedRoute = &_routes.front();
        if (!_selectedRoute->Waypoints.empty())
            _selectedWaypoint = _selectedRoute->Waypoints[0];
    }

    _selectedRacingLine = _navigation->RacingLines.empty() ? -1 : 0;
    _selectedRacingLineSegment = -1;
    if (_selectedRacingLine != -1 && _selectedRacingLine < static_cast<int>(_navigation->RacingLines.size()))
    {
        auto const& line = _navigation->RacingLines[static_cast<std::size_t>(_selectedRacingLine)];
        if (line && !line->Segments.empty())
            _selectedRacingLineSegment = 0;
    }

    std::cout << "[CharmyBee] Navigation rendering configured." << std::endl;
}

void CharmyBee::OnWorkspaceLoad()
{
    auto* scene = Editor::SceneManager::Current();
    if (scene == nullptr)
        return;

    _database = &scene->Database;
    if (_database == nullptr || Editor::SceneManager::DisableRendering)
        return;

    ResetAssetTree();

    std::string name = scene->SourceFileName;
    if (name.empty())
        name = "Unnamed Workspace";
    _title = "Sumo Engine Editor - " + name + " <OpenGL>";
    std::cout << "[CharmyBee] Workspace loaded: " << _title << std::endl;

    for (auto* definition : _database->RootDefinitions)
    {
        if (definition == nullptr)
            continue;

        std::string nodeName = definition->ShortName.empty() ? definition->UidName : definition->ShortName;
        AddItemNode(nodeName, definition);
    }

    UpdateTriggerPhantomBoxes();
}

void CharmyBee::DrawNodeCreationMenu()
{
    if (ImGui::MenuItem("Create Empty Folder", "Ctrl+Shift+N"))
        std::cout << "[CharmyBee] Create Empty Folder requested." << std::endl;
}

void CharmyBee::ResetAssetTree()
{
    _assetRoot = std::make_unique<TreeNode>("assets", true);
    _selectedFolder = _assetRoot.get();
}

std::vector<std::string> CharmyBee::GetPathComponents(std::string const& value) const
{
    std::vector<std::string> parts;
    parts.reserve(8);

    std::string segment;
    segment.reserve(value.size());
    for (char ch : value)
    {
        if (ch == '/' || ch == '\\')
        {
            if (!segment.empty())
            {
                parts.push_back(segment);
                segment.clear();
            }
            continue;
        }

        segment.push_back(ch);
    }

    if (!segment.empty())
        parts.push_back(segment);

    return parts;
}

CharmyBee::TreeNode* CharmyBee::AddFolderNode(std::string const& path)
{
    if (_assetRoot == nullptr)
        ResetAssetTree();

    if (path.empty())
        return _assetRoot.get();

    std::string normalized = path;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');
    while (!normalized.empty() && normalized.back() == '/')
        normalized.pop_back();

    TreeNode* parent = _assetRoot.get();
    for (auto const& component : GetPathComponents(normalized))
    {
        auto it = std::find_if(parent->Children.begin(), parent->Children.end(),
            [&](std::unique_ptr<TreeNode> const& child) {
                return child->IsFolder && child->Name == component;
            });

        if (it != parent->Children.end())
        {
            parent = it->get();
            continue;
        }

        auto folder = std::make_unique<TreeNode>(component, true);
        TreeNode* folderPtr = folder.get();
        parent->Children.push_back(std::move(folder));
        parent = folderPtr;
    }

    return parent;
}

void CharmyBee::AddItemNode(std::string const& name, SlLib::Resources::Scene::SeDefinitionNode* definition)
{
    if (name.empty())
        return;

    if (_assetRoot == nullptr)
        ResetAssetTree();

    std::string normalized = name;
    std::replace(normalized.begin(), normalized.end(), '\\', '/');

    TreeNode* parent = _assetRoot.get();
    auto lastSlash = normalized.find_last_of('/');
    if (lastSlash != std::string::npos)
        parent = AddFolderNode(normalized.substr(0, lastSlash));

    std::string nodeName = lastSlash == std::string::npos ? normalized : normalized.substr(lastSlash + 1);
    if (nodeName.empty())
        nodeName = normalized;

    auto node = std::make_unique<TreeNode>(nodeName);
    node->Association = definition;
    parent->Children.push_back(std::move(node));

    std::cout << "[CharmyBee] Queued asset node: " << name << std::endl;
}

void CharmyBee::RenderAssetView()
{
    if (_assetRoot == nullptr)
        return;

    ImGui::BeginChild("Folder View", ImVec2(150.0f, 0.0f), true);

    std::function<void(TreeNode*)> drawFolder;
    drawFolder = [&](TreeNode* folder) {
        if (folder == nullptr)
            return;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (folder->Children.empty())
            flags |= ImGuiTreeNodeFlags_Leaf;
        if (_selectedFolder == folder)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(folder), flags, "%s", folder->Name.c_str());
        if (ImGui::IsItemClicked())
            _selectedFolder = folder;

        if (open)
        {
            for (auto& child : folder->Children)
            {
                if (child->IsFolder)
                    drawFolder(child.get());
            }

            ImGui::TreePop();
        }
    };

    drawFolder(_assetRoot.get());
    ImGui::EndChild();

    ImGui::SameLine();

    ImGui::BeginChild("Item View", ImVec2(0.0f, 0.0f), true);
    if (_selectedFolder != nullptr)
    {
        for (auto& child : _selectedFolder->Children)
        {
            if (child->IsFolder)
                continue;

            bool selected = child->Association != nullptr && Editor::Selection::ActiveNode == child->Association;
            if (ImGui::Selectable(child->Name.c_str(), selected))
            {
                if (child->Association != nullptr)
                    Editor::Selection::ActiveNode = child->Association;
            }
        }
    }
    ImGui::EndChild();
}

void CharmyBee::RenderSceneView()
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("Scene");

    if (_scenePanel)
        _scenePanel->OnImGuiRender();

    if (ImGui::Checkbox("Draw killzones", &_drawTriggerBoxes))
        UpdateTriggerPhantomBoxes();

    if (_debugKeyInput)
        PollGlfwKeyInput();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    _localSceneFrameWidth = avail.x;
    _localSceneFrameHeight = avail.y;

    ImGui::Dummy(avail);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    ImGui::GetWindowDrawList()->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_WindowBg));
    ImGui::GetWindowDrawList()->AddText(ImVec2(min.x + 10.0f, min.y + 10.0f),
                                        ImGui::GetColorU32(ImGuiCol_TextDisabled),
                                        "Scene rendering placeholder");

    if (_drawNavigation && !_sifNavigation)
    {
        if (_navigationTool)
            _navigationTool->OnRender();
    }

    // Free-fly controls (right-drag look) when hovering scene.
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f;
    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                          ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    _sceneViewHovered = hovered;

    if (_debugKeyInput)
        PollGlfwKeyInput();

    ImGui::End();
    ImGui::PopStyleVar();
}

void CharmyBee::UpdateOrbitFromInput(float delta)
{
    if (_controller == nullptr)
        return;

    GLFWwindow* window = _controller->Window();
    if (window == nullptr)
        return;

    if (_blockSceneInput)
    {
        _mouseOrbitTracking = false;
        return;
    }

    auto isDown = [&](int key) {
        int state = glfwGetKey(window, key);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    };

    float rotSpeed = 1.8f;
    float moveSpeed = _movementSpeed;

    _movementSpeed = std::clamp(_movementSpeed, 0.05f, 200.0f);

    double cursorX = 0.0;
    double cursorY = 0.0;
    glfwGetCursorPos(window, &cursorX, &cursorY);
    SlLib::Math::Vector2 cursorPos{static_cast<float>(cursorX), static_cast<float>(cursorY)};
    bool windowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED) == GLFW_TRUE;
    if (windowFocused)
    {
        auto isMouseDown = [&](int button) {
            int state = glfwGetMouseButton(window, button);
            return state == GLFW_PRESS || state == GLFW_REPEAT;
        };
        if (isMouseDown(GLFW_MOUSE_BUTTON_LEFT))
        {
            SlLib::Math::Vector2 current = cursorPos;
            if (_mouseOrbitTracking)
            {
                SlLib::Math::Vector2 delta = {current.X - _mouseOrbitLastPos.X, current.Y - _mouseOrbitLastPos.Y};
                constexpr float mouseSensitivity = 0.01f;
                _orbitYaw   += delta.X * mouseSensitivity;
                _orbitPitch -= delta.Y * mouseSensitivity;
                if (_debugKeyInput && (std::abs(delta.X) > 0.0f || std::abs(delta.Y) > 0.0f))
                {
                    std::cout << "[CharmyBee][MouseDrag] dx=" << delta.X << " dy=" << delta.Y << std::endl;
                }
            }
            else
            {
                _mouseOrbitTracking = true;
            }
            _mouseOrbitLastPos = current;
        }
        else
        {
            _mouseOrbitTracking = false;
        }
    }
    else
    {
        _mouseOrbitTracking = false;
    }

    float cp = std::cos(_orbitPitch);
    float sp = std::sin(_orbitPitch);
    float cy = std::cos(_orbitYaw);
    float sy = std::sin(_orbitYaw);

    SlLib::Math::Vector3 forward{cp * cy, sp, cp * sy};
    SlLib::Math::Vector3 right{-sy, 0.0f, cy};
    SlLib::Math::Vector3 up{0.0f, 1.0f, 0.0f};

    if (isDown(GLFW_KEY_J)) _orbitYaw   -= rotSpeed * delta;
    if (isDown(GLFW_KEY_L)) _orbitYaw   += rotSpeed * delta;
    if (isDown(GLFW_KEY_I)) _orbitPitch -= rotSpeed * delta;
    if (isDown(GLFW_KEY_K)) _orbitPitch += rotSpeed * delta;
    if (isDown(GLFW_KEY_H))
    {
        if (!_originAxesToggleKeyDown)
        {
            _originAxesToggleKeyDown = true;
            _drawOriginAxes = !_drawOriginAxes;
            _renderer.SetDrawOriginAxes(_drawOriginAxes);
        }
    }
    else
    {
        _originAxesToggleKeyDown = false;
    }
    if (isDown(GLFW_KEY_Z)) _movementSpeed /= 1.05f;
    if (isDown(GLFW_KEY_X)) _movementSpeed *= 1.05f;
    _movementSpeed = std::clamp(_movementSpeed, 0.05f, 200.0f);

    SlLib::Math::Vector3 deltaVec{0.0f, 0.0f, 0.0f};
    if (isDown(GLFW_KEY_W)) deltaVec = deltaVec + forward;
    if (isDown(GLFW_KEY_S)) deltaVec = deltaVec - forward;
    if (isDown(GLFW_KEY_A)) deltaVec = deltaVec - right;
    if (isDown(GLFW_KEY_D)) deltaVec = deltaVec + right;

    if (deltaVec.X != 0.0f || deltaVec.Y != 0.0f || deltaVec.Z != 0.0f)
    {
        float len = SlLib::Math::length(deltaVec);
        if (len > 0.0f)
            deltaVec = deltaVec * (1.0f / len);
        deltaVec = deltaVec * (moveSpeed * delta);
        _orbitTarget = _orbitTarget + deltaVec;
    }

    _orbitPitch = std::clamp(_orbitPitch, -1.4f, 1.4f);
    _orbitDistance = std::max(0.0f, _orbitDistance);
    _orbitOffset = {0.0f, 0.0f, 0.0f};

    _renderer.SetOrbitCamera(_orbitYaw, _orbitPitch, 0.0f, _orbitTarget);
    _renderer.SetDrawCollisionMesh(_drawCollisionMesh);
}

void CharmyBee::RenderRacingLineEditor()
{
    ImGui::Begin("Navigation");

    if (_navigation == nullptr)
    {
        ImGui::TextDisabled("Navigation data is missing.");
    }
    else
    {
        ImGui::Text("Routes: %zu", _routes.size());
        ImGui::Text("Waypoints: %zu", _navigation->Waypoints.size());
        ImGui::Text("Racing lines: %zu", _navigation->RacingLines.size());
        if (_selectedRoute != nullptr)
            ImGui::Text("Selected route: %d (%zu waypoints)", _selectedRoute->Id,
                        _selectedRoute->Waypoints.size());
        else
            ImGui::Text("Selected route: none");

        ImGui::Text("Selected racing line: %d", _selectedRacingLine);
    }

    ImGui::End();
}

void CharmyBee::RenderSifViewer()
{
    constexpr float kSifWidth = 560.0f;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    ImVec2 workSize = viewport ? viewport->WorkSize : ImVec2(1280.0f, 720.0f);
    float windowHeight = workSize.y - 20.0f;
    if (windowHeight < 200.0f)
        windowHeight = 200.0f;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + workSize.x - kSifWidth, workPos.y + 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kSifWidth, windowHeight), ImGuiCond_Always);
    if (!ImGui::Begin("SIF Viewer"))
    {
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        _blockSceneInput = true;

    if (ImGui::BeginTabBar("SifTabs"))
    {
        if (ImGui::BeginTabItem("SIF Viewer"))
        {
            if (_sifFilePath.empty())
            {
                ImGui::TextDisabled("No SIF/ZIF/SIG/ZIG file loaded. Use File > Load SIF/ZIF/SIG/ZIG File...");
            }
            else
            {
                ImGui::TextWrapped("File: %s", _sifFilePath.c_str());
                if (!_sifLoadMessage.empty())
                    ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "%s", _sifLoadMessage.c_str());

                ImGui::Text("Original size: %llu bytes", static_cast<unsigned long long>(_sifOriginalSize));
                if (_sifFileCompressed)
                    ImGui::Text("Decompressed payload: %llu bytes", static_cast<unsigned long long>(_sifDecompressedSize));

                {
                    std::scoped_lock lock(_unityExportMutex);
                    if (!_unityExportStatus.empty())
                        ImGui::TextWrapped("Unity export: %s", _unityExportStatus.c_str());
                }
                if (ImGui::Button("Export SIF to Unity"))
                    ExportSifToUnity();

                ImGui::Separator();
                if (!_sifChunks.empty())
                {
                    ImGui::Text("Chunks: %zu", _sifChunks.size());
                    ImGui::BeginChild("chunkList", ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, 200.0f), true);
                    for (std::size_t i = 0; i < _sifChunks.size(); ++i)
                    {
                        bool selected = static_cast<int>(i) == _selectedChunk;
                        char label[128];
                        std::snprintf(label, sizeof(label), "[%02zu] %s (0x%08X)", i, _sifChunks[i].Name.c_str(),
                                      _sifChunks[i].TypeValue);
                        if (ImGui::Selectable(label, selected))
                            _selectedChunk = static_cast<int>(i);
                    }
                    ImGui::EndChild();
                    ImGui::SameLine();
                    ImGui::BeginChild("chunkDetails", ImVec2(0, 200.0f), true);
                    if (_selectedChunk >= 0 && _selectedChunk < static_cast<int>(_sifChunks.size()))
                    {
                        auto const& chunk = _sifChunks[static_cast<std::size_t>(_selectedChunk)];
                        ImGui::Text("Name: %s", chunk.Name.c_str());
                        ImGui::Text("Type: 0x%08X", chunk.TypeValue);
                        ImGui::Text("Data: %u bytes", chunk.DataSize);
                        ImGui::Text("Chunk: %u bytes", chunk.ChunkSize);
                        ImGui::Text("Relocations: %zu", chunk.Relocations.size());
                        ImGui::Text("Endian: %s", chunk.BigEndian ? "Big" : "Little");
                        ImGui::Separator();
                        if (chunk.TypeValue == MakeTypeCode('C', 'O', 'L', 'I'))
                        {
                            if (ImGui::Button("Show Collision Mesh"))
                            {
                                _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                                LoadCollisionDebugGeometry();
                            }
                            ImGui::SameLine();
                            ImGui::Checkbox("Draw", &_drawCollisionMesh);
                        }
                         else if (chunk.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'))
                         {
                             if (ImGui::Button("Show Forest Meshes"))
                             {
                                 _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                                 LoadForestResources();
                                 _drawForestMeshes = true;
                                 UpdateForestMeshRendering();
                             }
                             if (ImGui::Checkbox("Draw", &_drawForestMeshes))
                                 UpdateForestMeshRendering();
                             if (ImGui::Checkbox("Draw boxes", &_drawForestBoxes))
                                 UpdateForestBoxRenderer();
                            if (ImGui::Button("Export track.Forest OBJ"))
                            {
                                SeEditor::Dialogs::FileDialogOptions options;
                                options.Title = "Export track.Forest OBJ";
                                options.FilterPatterns = {"*.obj"};
                                options.FilterDescription = "Wavefront OBJ";
                                options.DefaultPathAndFile = _sifFilePath;
                                if (!options.DefaultPathAndFile.empty())
                                {
                                    std::filesystem::path base = options.DefaultPathAndFile;
                                    base.replace_extension(".obj");
                                    options.DefaultPathAndFile = base.string();
                                }
                                if (auto result = SeEditor::Dialogs::TinyFileDialog::saveFileDialog(options))
                                    ExportForestObj(*result, "track.Forest");
                            }
                        }
                        else if (chunk.TypeValue == MakeTypeCode('T', 'R', 'A', 'K'))
                        {
                            if (ImGui::Button("Show Navigation"))
                            {
                                _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                                LoadNavigationResources();
                            }
                            if (ImGui::Checkbox("Draw", &_drawNavigation))
                                UpdateDebugLines();
                            if (_sifNavigationTool)
                            {
                                if (ImGui::Checkbox("Waypoints", &_drawNavigationWaypoints))
                                    UpdateDebugLines();
                                if (_drawNavigationWaypoints)
                                {
                                    ImGui::SetNextItemWidth(80.0f);
                                    if (ImGui::DragFloat("Size", &_navigationWaypointBoxSize, 0.1f, 0.2f, 50.0f, "%.1f"))
                                        UpdateDebugLines();
                                }
                            }
                        }
                        else if (chunk.TypeValue == MakeTypeCode('L', 'O', 'G', 'C'))
                        {
                            if (ImGui::Button("Show Logic"))
                            {
                                _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                                LoadLogicResources();
                            }
                            if (ImGui::Checkbox("Draw", &_drawLogic))
                            {
                                UpdateDebugLines();
                                UpdateForestMeshRendering();
                            }
                        }
                        else
                        {
                            ImGui::TextDisabled("No visualizer for this chunk yet.");
                        }
                    }
                    ImGui::EndChild();
                    ImGui::Separator();
                }

                bool showHex = !_sifParseError.empty() || _sifChunks.empty();
                if (showHex)
                {
                    if (!_sifParseError.empty())
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Parse error: %s", _sifParseError.c_str());
                    ImGui::TextWrapped("Showing raw hex dump of the data.");
                    ImGui::BeginChild("SifContents", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
                    for (auto const& line : _sifHexDump)
                        ImGui::TextUnformatted(line.c_str());
                    ImGui::EndChild();
                }
                else
                {
                    ImGui::Text("Chunks: %zu", _sifChunks.size());
                    ImGui::BeginChild("SifContents", ImVec2(0.0f, 0.0f), true, ImGuiWindowFlags_HorizontalScrollbar);
                    for (auto const& chunk : _sifChunks)
                    {
                        ImGui::Text("%s (0x%08X)", chunk.Name.c_str(), chunk.TypeValue);
                        ImGui::Text("  Data: %u bytes, Chunk: %u bytes, Relocations: %zu", chunk.DataSize,
                                    chunk.ChunkSize, chunk.Relocations.size());
                        std::string relocLine = FormatRelocationList(chunk.Relocations);
                        if (!relocLine.empty())
                            ImGui::TextUnformatted(relocLine.c_str());
                        ImGui::Separator();
                    }
                    ImGui::EndChild();
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Hierarchy"))
        {
            if (_selectedChunk < 0 || _selectedChunk >= static_cast<int>(_sifChunks.size()))
            {
                ImGui::TextDisabled("Select a chunk to view its hierarchy.");
            }
            else
            {
                auto const& chunk = _sifChunks[static_cast<std::size_t>(_selectedChunk)];
                if (chunk.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'))
                {
                    if (_forestHierarchy.empty())
                    {
                        ImGui::Text("No forest data loaded yet.");
                        if (ImGui::Button("Load Forest"))
                        {
                            _selectedChunk = static_cast<int>(&chunk - &_sifChunks[0]);
                            LoadForestResources();
                            UpdateForestHierarchy();
                            _showForestHierarchyWindow = true;
                        }
                    }
                    else
                    {
                        ImGui::BeginChild("ForestHierarchyTab", ImVec2(0.0f, 0.0f), true);
                        RenderForestHierarchyList();
                        ImGui::EndChild();
                    }
                }
                else if (chunk.TypeValue == MakeTypeCode('T', 'R', 'A', 'K'))
                {
                    if (_sifNavigation == nullptr || _sifNavigation->RacingLines.empty())
                    {
                        ImGui::Text("No navigation data loaded yet.");
                    }
                    else
                    {
                        bool changed = false;
                        for (auto& entry : _navigationLineEntries)
                        {
                            if (ImGui::Checkbox(entry.Name.c_str(), &entry.Visible))
                                changed = true;
                        }
                        if (changed)
                            UpdateNavigationLineVisibility();
                    }
                }
                else if (chunk.TypeValue == MakeTypeCode('L', 'O', 'G', 'C'))
                {
                    if (_sifLogic == nullptr)
                    {
                        ImGui::Text("No logic data loaded yet.");
                    }
                    else
                    {
                        bool changed = false;

                        if (ImGui::Button("Export LOGC Rewrite"))
                            ExportLogicRewrite();

                        if (ImGui::TreeNodeEx("Triggers", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            if (ImGui::Checkbox("Trigger Quads", &_drawLogicTriggers))
                                changed = true;
                            if (ImGui::Checkbox("Trigger Volumes", &_drawTriggerBoxes))
                                UpdateTriggerPhantomBoxes();
                            if (ImGui::Checkbox("Trigger Normals", &_drawLogicTriggerNormals))
                                changed = true;
                            if (_drawLogicTriggerNormals)
                            {
                                ImGui::SetNextItemWidth(80.0f);
                                if (ImGui::DragFloat("Normal Size", &_logicTriggerNormalSize, 0.2f, 0.5f, 100.0f, "%.1f"))
                                    changed = true;
                            }
                            if (!_logicTriggerGroups.empty())
                            {
                                ImGui::SeparatorText("Types");
                                for (auto& group : _logicTriggerGroups)
                                {
                                    std::string label = group.Name + " (" + std::to_string(group.Count) + ")";
                                    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
                                    bool open = ImGui::TreeNodeEx(label.c_str(), flags);
                                    ImGui::SameLine();
                                    std::string checkLabel = "##logic_type_" + std::to_string(group.Hash);
                                    if (ImGui::Checkbox(checkLabel.c_str(), &group.Visible))
                                        changed = true;
                                    if (open)
                                    {
                                        for (int idx : group.Indices)
                                        {
                                            if (idx < 0 || idx >= static_cast<int>(_sifLogic->Triggers.size()))
                                                continue;
                                            auto const& trig = _sifLogic->Triggers[static_cast<std::size_t>(idx)];
                                            if (!trig)
                                                continue;
                                            ImGui::Text("Trigger %d @ (%.3f, %.3f, %.3f)",
                                                        idx,
                                                        trig->Position.X,
                                                        trig->Position.Y,
                                                        trig->Position.Z);
                                        }
                                        ImGui::TreePop();
                                    }
                                }
                            }
                            ImGui::TreePop();
                        }

                        if (ImGui::TreeNodeEx("Locators", ImGuiTreeNodeFlags_DefaultOpen))
                        {
                            if (ImGui::Checkbox("Locator Meshes + Boxes", &_drawLogicLocators))
                                changed = true;
                            if (_drawLogicLocators)
                            {
                                ImGui::SetNextItemWidth(80.0f);
                                if (ImGui::DragFloat("Box Size", &_logicLocatorBoxSize, 0.1f, 0.2f, 50.0f, "%.1f"))
                                    changed = true;
                            }
                            if (ImGui::Checkbox("Locator Axes", &_drawLogicLocatorAxes))
                                changed = true;
                            if (_drawLogicLocatorAxes)
                            {
                                ImGui::SetNextItemWidth(80.0f);
                                if (ImGui::DragFloat("Axis Size", &_logicLocatorAxisSize, 0.2f, 0.5f, 100.0f, "%.1f"))
                                    changed = true;
                            }

                            ImGui::SeparatorText("Groups");
                            std::unordered_map<int, std::vector<int>> groups;
                            for (int i = 0; i < static_cast<int>(_sifLogic->Locators.size()); ++i)
                            {
                                auto const& locator = _sifLogic->Locators[static_cast<std::size_t>(i)];
                                if (!locator)
                                    continue;
                                groups[locator->GroupNameHash].push_back(i);
                            }
                            std::vector<int> groupKeys;
                            groupKeys.reserve(groups.size());
                            for (auto const& pair : groups)
                                groupKeys.push_back(pair.first);
                            std::sort(groupKeys.begin(), groupKeys.end());

                            for (int hash : groupKeys)
                            {
                                auto const& indices = groups[hash];
                                std::string label = "Group " + std::to_string(hash) + " (" + std::to_string(indices.size()) + ")";
                                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
                                {
                                    for (int idx : indices)
                                    {
                                        if (idx < 0 || idx >= static_cast<int>(_sifLogic->Locators.size()))
                                            continue;
                                        auto const& loc = _sifLogic->Locators[static_cast<std::size_t>(idx)];
                                        if (!loc)
                                            continue;
                                        ImGui::Text("Locator %d", idx);
                                        ImGui::SameLine();
                                        if (ImGui::SmallButton("Focus Camera"))
                                        {
                                            _orbitTarget = {loc->PositionAsFloats.X,
                                                            loc->PositionAsFloats.Y,
                                                            loc->PositionAsFloats.Z};
                                            _orbitOffset = {0.0f, 0.0f, 0.0f};
                                        }
                                        float pos[3] = {loc->PositionAsFloats.X,
                                                        loc->PositionAsFloats.Y,
                                                        loc->PositionAsFloats.Z};
                                        ImGui::PushID(idx);
                                        ImGui::SetNextItemWidth(80.0f);
                                        bool posChanged = ImGui::DragFloat("X", &pos[0], 0.1f);
                                        ImGui::SetNextItemWidth(80.0f);
                                        posChanged |= ImGui::DragFloat("Y", &pos[1], 0.1f);
                                        ImGui::SetNextItemWidth(80.0f);
                                        posChanged |= ImGui::DragFloat("Z", &pos[2], 0.1f);
                                        ImGui::PopID();

                                        if (posChanged)
                                        {
                                            loc->PositionAsFloats.X = pos[0];
                                            loc->PositionAsFloats.Y = pos[1];
                                            loc->PositionAsFloats.Z = pos[2];
                                            BuildLogicLocatorMeshes();
                                            UpdateForestMeshRendering();
                                            UpdateDebugLines();
                                        }
                                    }
                                    ImGui::TreePop();
                                }
                            }
                            ImGui::TreePop();
                        }

                        if (changed)
                        {
                            UpdateDebugLines();
                            UpdateForestMeshRendering();
                            if (_drawTriggerBoxes)
                                UpdateTriggerPhantomBoxes();
                        }
                    }
                }
                else
                {
                    ImGui::TextDisabled("No hierarchy for this chunk.");
                }
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Animator"))
        {
            bool animatorChanged = false;
            if (!_forestLibrary)
            {
                ImGui::TextDisabled("No forest data loaded yet.");
                if (ImGui::Button("Load Forest"))
                {
                    LoadForestResources();
                    _animatorDirty = true;
                    animatorChanged = true;
                }
            }
            else if (_forestLibrary->Forests.empty())
            {
                ImGui::TextDisabled("Forest list is empty.");
            }
            else
            {
                if (_animatorSelectedForest < 0)
                    _animatorSelectedForest = 0;
                if (static_cast<std::size_t>(_animatorSelectedForest) >= _forestLibrary->Forests.size())
                    _animatorSelectedForest = static_cast<int>(_forestLibrary->Forests.size()) - 1;

                auto const& forestEntry = _forestLibrary->Forests[static_cast<std::size_t>(_animatorSelectedForest)];
                std::string forestLabel = forestEntry.Name.empty()
                                              ? std::string("Forest ") + std::to_string(_animatorSelectedForest)
                                              : forestEntry.Name;

                if (ImGui::BeginCombo("Forest", forestLabel.c_str()))
                {
                    for (std::size_t i = 0; i < _forestLibrary->Forests.size(); ++i)
                    {
                        auto const& entry = _forestLibrary->Forests[i];
                        std::string name = entry.Name.empty()
                                               ? std::string("Forest ") + std::to_string(i)
                                               : entry.Name;
                        bool selected = static_cast<int>(i) == _animatorSelectedForest;
                        if (ImGui::Selectable(name.c_str(), selected))
                        {
                            _animatorSelectedForest = static_cast<int>(i);
                            _animatorSelectedTree = 0;
                            _animatorSelectedAnimation = -1;
                            _animatorSelectedBranch = 0;
                            _animatorFrame = 0;
                            _animatorTime = 0.0f;
                            _animatorFrameAccumulator = 0.0f;
                            _animatorDirty = true;
                            animatorChanged = true;
                        }
                        if (selected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                SeEditor::Forest::SuRenderTree* tree = nullptr;
                if (forestEntry.Forest && !forestEntry.Forest->Trees.empty())
                {
                    if (_animatorSelectedTree < 0)
                        _animatorSelectedTree = 0;
                    if (static_cast<std::size_t>(_animatorSelectedTree) >= forestEntry.Forest->Trees.size())
                        _animatorSelectedTree = static_cast<int>(forestEntry.Forest->Trees.size()) - 1;

                    tree = forestEntry.Forest->Trees[static_cast<std::size_t>(_animatorSelectedTree)].get();
                    std::string treeLabel = tree && tree->Hash != 0
                                                ? std::string("Tree ") + std::to_string(tree->Hash)
                                                : std::string("Tree ") + std::to_string(_animatorSelectedTree);
                    if (ImGui::BeginCombo("Tree", treeLabel.c_str()))
                    {
                        for (std::size_t i = 0; i < forestEntry.Forest->Trees.size(); ++i)
                        {
                            auto const& treeEntry = forestEntry.Forest->Trees[i];
                            std::string name = treeEntry && treeEntry->Hash != 0
                                                   ? std::string("Tree ") + std::to_string(treeEntry->Hash)
                                                   : std::string("Tree ") + std::to_string(i);
                            bool selected = static_cast<int>(i) == _animatorSelectedTree;
                            if (ImGui::Selectable(name.c_str(), selected))
                            {
                                _animatorSelectedTree = static_cast<int>(i);
                                _animatorSelectedAnimation = -1;
                                _animatorSelectedBranch = 0;
                                _animatorFrame = 0;
                                _animatorTime = 0.0f;
                                _animatorFrameAccumulator = 0.0f;
                                _animatorDirty = true;
                                animatorChanged = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::TextDisabled("Selected forest has no trees.");
                }

                SeEditor::Forest::SuAnimation* animation = nullptr;
                if (tree)
                {
                    std::string animLabel = "None";
                    if (_animatorSelectedAnimation >= 0 &&
                        static_cast<std::size_t>(_animatorSelectedAnimation) < tree->AnimationEntries.size())
                    {
                        auto const& animEntry = tree->AnimationEntries[static_cast<std::size_t>(_animatorSelectedAnimation)];
                        animLabel = animEntry.AnimName.empty()
                                        ? std::string("Anim ") + std::to_string(_animatorSelectedAnimation)
                                        : animEntry.AnimName;
                        animation = animEntry.Animation.get();
                    }

                    if (ImGui::BeginCombo("Animation", animLabel.c_str()))
                    {
                        bool noneSelected = _animatorSelectedAnimation < 0;
                        if (ImGui::Selectable("None", noneSelected))
                        {
                            _animatorSelectedAnimation = -1;
                            _animatorPlaying = false;
                            _animatorFrame = 0;
                            _animatorTime = 0.0f;
                            _animatorFrameAccumulator = 0.0f;
                            _animatorDirty = true;
                            animatorChanged = true;
                        }
                        if (noneSelected)
                            ImGui::SetItemDefaultFocus();

                        for (std::size_t i = 0; i < tree->AnimationEntries.size(); ++i)
                        {
                            auto const& animEntry = tree->AnimationEntries[i];
                            std::string name = animEntry.AnimName.empty()
                                                   ? std::string("Anim ") + std::to_string(i)
                                                   : animEntry.AnimName;
                            bool selected = static_cast<int>(i) == _animatorSelectedAnimation;
                            if (ImGui::Selectable(name.c_str(), selected))
                            {
                                _animatorSelectedAnimation = static_cast<int>(i);
                                _animatorPlaying = false;
                                _animatorFrame = 0;
                                _animatorTime = 0.0f;
                                _animatorFrameAccumulator = 0.0f;
                                _animatorDirty = true;
                                animatorChanged = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                if (tree && !tree->Branches.empty())
                {
                    int branchCount = static_cast<int>(tree->Branches.size());
                    if (_animatorSelectedBranch < 0)
                        _animatorSelectedBranch = 0;
                    if (_animatorSelectedBranch >= branchCount)
                        _animatorSelectedBranch = branchCount - 1;
                    auto const& branch = tree->Branches[static_cast<std::size_t>(_animatorSelectedBranch)];
                    std::string branchLabel = branch && !branch->Name.empty()
                                                  ? branch->Name
                                                  : std::string("Branch ") + std::to_string(_animatorSelectedBranch);
                    if (ImGui::BeginCombo("Branch", branchLabel.c_str()))
                    {
                        for (int i = 0; i < branchCount; ++i)
                        {
                            auto const& branchEntry = tree->Branches[static_cast<std::size_t>(i)];
                            std::string name = branchEntry && !branchEntry->Name.empty()
                                                   ? branchEntry->Name
                                                   : std::string("Branch ") + std::to_string(i);
                            bool selected = i == _animatorSelectedBranch;
                            if (ImGui::Selectable(name.c_str(), selected))
                            {
                                _animatorSelectedBranch = i;
                                animatorChanged = true;
                            }
                            if (selected)
                                ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }

                if (animation)
                {
                    if (animation->Type < 0x06 || animation->Type > 0x0A)
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Unsupported animation type: %d", animation->Type);
                    }
                    else
                    {
                        if (ImGui::Button("Only Render this"))
                        {
                            for (auto& forest : _forestHierarchy)
                            {
                                forest.Visible = false;
                                for (auto& treeEntry : forest.Trees)
                                    treeEntry.Visible = false;
                            }
                            if (_animatorSelectedForest >= 0 &&
                                static_cast<std::size_t>(_animatorSelectedForest) < _forestHierarchy.size())
                            {
                                auto& forest = _forestHierarchy[static_cast<std::size_t>(_animatorSelectedForest)];
                                forest.Visible = true;
                                if (_animatorSelectedTree >= 0 &&
                                    static_cast<std::size_t>(_animatorSelectedTree) < forest.Trees.size())
                                {
                                    forest.Trees[static_cast<std::size_t>(_animatorSelectedTree)].Visible = true;
                                }
                            }
                            ApplyTreeVisibilityToLayers();
                            UpdateForestBoxRenderer();
                            UpdateForestMeshRendering();
                            SaveForestHierarchyVisibility();
                        }

                        if (ImGui::Button(_animatorPlaying ? "Pause" : "Play"))
                        {
                            _animatorPlaying = !_animatorPlaying;
                            _animatorTime = static_cast<float>(_animatorFrame) / _animatorFps;
                            _animatorFrameAccumulator = 0.0f;
                            animatorChanged = true;
                        }
                        ImGui::SameLine();
                        ImGui::Text("Frames: %d", animation->NumFrames);

                        if (animation->NumFrames > 0)
                        {
                            int frameMin = 0;
                            int frameMax = std::max(0, animation->NumFrames - 1);
                            if (ImGui::SliderInt("Frame", &_animatorFrame, frameMin, frameMax))
                            {
                                _animatorTime = static_cast<float>(_animatorFrame) / _animatorFps;
                                _animatorFrameAccumulator = 0.0f;
                                _animatorDirty = true;
                                animatorChanged = true;
                            }
                            float norm = frameMax > 0 ? static_cast<float>(_animatorFrame) / static_cast<float>(frameMax) : 0.0f;
                            ImGui::Text("Time: %.3f (normalized %.3f)", _animatorTime, norm);
                            if (ImGui::Checkbox("Render bones", &_drawAnimatorBones))
                                animatorChanged = true;
                        }

                        if (tree && !animation->SamplesDecoded)
                        {
                            if (!animation->DecodeType6Samples(*tree))
                                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.4f, 1.0f), "Failed to decode Type-6 data.");
                            else
                                _animatorDirty = true;
                        }

                        if (animation->SamplesDecoded &&
                            _animatorSelectedBranch >= 0 &&
                            static_cast<std::size_t>(_animatorSelectedBranch) < animation->Quantization.Translation.size())
                        {
                            auto showRange = [](char const* label, SeEditor::Forest::SuAnimationQuantRange const& range) {
                                if (range.Valid)
                                    ImGui::Text("%s min=%.6f delta=%.6f", label, range.Minimum, range.Delta);
                                else
                                    ImGui::TextDisabled("%s default", label);
                            };

                            auto const& qt = animation->Quantization.Translation[static_cast<std::size_t>(_animatorSelectedBranch)];
                            auto const& qr = animation->Quantization.Rotation[static_cast<std::size_t>(_animatorSelectedBranch)];
                            auto const& qs = animation->Quantization.Scale[static_cast<std::size_t>(_animatorSelectedBranch)];
                            auto const& qv = animation->Quantization.Visibility[static_cast<std::size_t>(_animatorSelectedBranch)];

                            ImGui::SeparatorText("Quantization");
                            showRange("T.X", qt[0]);
                            showRange("T.Y", qt[1]);
                            showRange("T.Z", qt[2]);
                            showRange("R.X", qr[0]);
                            showRange("R.Y", qr[1]);
                            showRange("R.Z", qr[2]);
                            showRange("R.W", qr[3]);
                            showRange("S.X", qs[0]);
                            showRange("S.Y", qs[1]);
                            showRange("S.Z", qs[2]);
                            showRange("Vis", qv);
                        }
                    }
                }
            }
            if (animatorChanged)
                SaveAnimatorSettings();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

void CharmyBee::ExportSifToUnity()
{
    if (_unityExportBusy.exchange(true))
        return;

    if (_unityExportWorker && _unityExportWorker->joinable())
        _unityExportWorker->join();

    const std::filesystem::path sifPath = std::filesystem::path(_sifFilePath);
    const std::filesystem::path exportRoot = GetStuffRoot();

    {
        std::scoped_lock lock(_unityExportMutex);
        _unityExportStatus = "Running... (root: " + exportRoot.string() + ")";
    }

    _unityExportWorker = std::make_unique<std::thread>([this, sifPath, exportRoot]() {
        auto res = SeEditor::UnityExport::ExportSifToUnity(sifPath, exportRoot);
        {
            std::scoped_lock lock(_unityExportMutex);
            if (res.Success)
            {
                std::ostringstream msg;
                msg << "Done: " << res.ExportJsonPath.string();
                if (!res.ScenePath.empty())
                    msg << " | Scene: " << res.ScenePath.string();
                _unityExportStatus = msg.str();
            }
            else
            {
                _unityExportStatus = "Failed: " + res.Error;
            }
        }
        _unityExportBusy.store(false);
    });
}

std::filesystem::path CharmyBee::GetStuffRoot() const
{
    std::filesystem::path root;
    if (_stuffRootOverride && !_stuffRootOverride->empty())
    {
        root = *_stuffRootOverride;
    }
    else
    {
        root = std::filesystem::current_path() / "CppSLib_Stuff";
    }
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    return root;
}

void CharmyBee::RenderStuffTreeNode(std::filesystem::path const& path)
{
    std::error_code ec;
    bool isDir = std::filesystem::is_directory(path, ec);
    std::string label = path.filename().empty() ? path.string() : path.filename().string();

    if (isDir)
    {
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        bool open = ImGui::TreeNodeEx(label.c_str(), flags);
        if (open)
        {
            std::vector<std::filesystem::path> dirs;
            std::vector<std::filesystem::path> files;
            for (auto const& entry : std::filesystem::directory_iterator(path, ec))
            {
                if (ec)
                    break;
                if (entry.is_directory())
                    dirs.push_back(entry.path());
                else
                    files.push_back(entry.path());
            }
            auto sorter = [](std::filesystem::path const& a, std::filesystem::path const& b) {
                return a.filename().string() < b.filename().string();
            };
            std::sort(dirs.begin(), dirs.end(), sorter);
            std::sort(files.begin(), files.end(), sorter);

            for (auto const& dir : dirs)
                RenderStuffTreeNode(dir);
            for (auto const& file : files)
            {
                ImGui::PushID(file.string().c_str());
                ImGui::Selectable(file.filename().string().c_str());
                if (ImGui::BeginPopupContextItem())
                {
                    std::string ext = file.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                        return static_cast<char>(std::tolower(ch));
                    });
                    if (ext == ".zif" || ext == ".zig")
                    {
                        if (ImGui::MenuItem("Decompress (auto)"))
                            DecompressZifZigFile(file);
                    }
                    else if (ext == ".sif" || ext == ".sig")
                    {
                        if (ImGui::MenuItem("Load SIF"))
                            LoadSifFile(file);
                        if (ImGui::MenuItem("Re-ZIF (PC)"))
                            RecompressToZifZig(file, false, false);
                        if (ImGui::MenuItem("Re-ZIF (PS3)"))
                            RecompressToZifZig(file, false, true);
                        if (ImGui::MenuItem("Re-ZIG (PC)"))
                            RecompressToZifZig(file, true, false);
                        if (ImGui::MenuItem("Re-ZIG (PS3)"))
                            RecompressToZifZig(file, true, true);
                    }
                    ImGui::EndPopup();
                }
                ImGui::PopID();
            }

            ImGui::TreePop();
        }
    }
    else
    {
        ImGui::PushID(path.string().c_str());
        ImGui::Selectable(label.c_str());
        if (ImGui::BeginPopupContextItem())
        {
            std::string ext = path.extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
                return static_cast<char>(std::tolower(ch));
            });
            if (ext == ".zif" || ext == ".zig")
            {
                if (ImGui::MenuItem("Decompress (auto)"))
                    DecompressZifZigFile(path);
            }
            else if (ext == ".sif" || ext == ".sig")
            {
                if (ImGui::MenuItem("Load SIF"))
                    LoadSifFile(path);
                if (ImGui::MenuItem("Re-ZIF (PC)"))
                    RecompressToZifZig(path, false, false);
                if (ImGui::MenuItem("Re-ZIF (PS3)"))
                    RecompressToZifZig(path, false, true);
                if (ImGui::MenuItem("Re-ZIG (PC)"))
                    RecompressToZifZig(path, true, false);
                if (ImGui::MenuItem("Re-ZIG (PS3)"))
                    RecompressToZifZig(path, true, true);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
}

void CharmyBee::RenderStuffSifVirtualTree(std::filesystem::path const& root)
{
    std::filesystem::path xpacRoot = root / "xpac";
    std::error_code ec;
    if (!std::filesystem::exists(xpacRoot, ec))
        return;

    if (_stuffSifCacheDirty.load() || _stuffSifCacheRoot != root)
        BuildStuffSifCache(root);

    if (!ImGui::TreeNodeEx("SIF Files", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
        return;

    for (auto const& group : _stuffSifCache)
    {
        if (!ImGui::TreeNodeEx(group.XpacName.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
            continue;

        for (auto const& entry : group.Entries)
        {
            ImGui::PushID(entry.Path.string().c_str());
            ImGui::Selectable(entry.Label.c_str());
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Load SIF"))
                    LoadSifFile(entry.Path);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    ImGui::TreePop();
}

void CharmyBee::BuildStuffSifCache(std::filesystem::path const& root)
{
    _stuffSifCacheRoot = root;
    _stuffSifCache.clear();
    std::filesystem::path xpacRoot = root / "xpac";
    std::error_code ec;
    if (!std::filesystem::exists(xpacRoot, ec))
    {
        _stuffSifCacheDirty = false;
        return;
    }

    std::vector<std::filesystem::path> xpacs;
    for (auto const& entry : std::filesystem::directory_iterator(xpacRoot, ec))
    {
        if (ec)
            break;
        if (entry.is_directory())
            xpacs.push_back(entry.path());
    }
    std::sort(xpacs.begin(), xpacs.end(), [](auto const& a, auto const& b) {
        return a.filename().string() < b.filename().string();
    });

    for (auto const& xpacDir : xpacs)
    {
        CachedSifGroup group;
        group.XpacName = xpacDir.filename().string();

        std::vector<std::filesystem::path> sifs;
        for (auto const& entry : std::filesystem::recursive_directory_iterator(xpacDir, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;
            if (SeStricmp(entry.path().extension().string().c_str(), ".sif") == 0)
                sifs.push_back(entry.path());
        }
        std::sort(sifs.begin(), sifs.end(), [](auto const& a, auto const& b) {
            return a.filename().string() < b.filename().string();
        });

        for (auto const& sifPath : sifs)
        {
            std::filesystem::path relPath = std::filesystem::relative(sifPath, xpacDir, ec);
            std::vector<std::string> parts;
            for (auto const& part : relPath)
            {
                std::string seg = part.string();
                if (seg.empty())
                    continue;
                if (SeStricmp(seg.c_str(), "_Resource") == 0)
                    continue;
                if (!seg.empty() && seg[0] == '_')
                    seg.erase(seg.begin());
                if (!seg.empty())
                    parts.push_back(seg);
            }
            std::string label;
            if (parts.size() <= 1)
            {
                label = parts.empty() ? std::string() : parts.back();
            }
            else
            {
                for (std::size_t i = 1; i < parts.size(); ++i)
                {
                    if (i > 1)
                        label += "/";
                    label += parts[i];
                }
            }
            if (label.empty())
                label = sifPath.filename().string();

            group.Entries.push_back({sifPath, label});
        }

        _stuffSifCache.push_back(std::move(group));
    }

    _stuffSifCacheDirty = false;
}

void CharmyBee::DecompressZifZigFile(std::filesystem::path const& path)
{
    std::vector<std::uint8_t> input;
    if (!ReadFileBytes(path, input))
    {
        _xpacStatus = "Failed to read " + path.string();
        return;
    }

    std::string error;
    std::vector<std::uint8_t> output;
    if (!SeEditor::Xpac::DecodeZifZig(input, output, error) || output.empty())
    {
        _xpacStatus = "Decode failed: " + error;
        return;
    }

    std::filesystem::path outPath = path;
    std::string ext = outPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (ext == ".zif")
        outPath.replace_extension(".sif");
    else if (ext == ".zig")
        outPath.replace_extension(".sig");
    else
        outPath.replace_extension(".bin");

    if (!WriteFileBytes(outPath, output))
    {
        _xpacStatus = "Failed to write " + outPath.string();
        return;
    }

    _xpacStatus = "Decoded " + path.filename().string() + " -> " + outPath.filename().string();
}

void CharmyBee::RecompressToZifZig(std::filesystem::path const& path, bool zig, bool ps3)
{
    std::vector<std::uint8_t> input;
    if (!ReadFileBytes(path, input))
    {
        _xpacStatus = "Failed to read " + path.string();
        return;
    }

    std::vector<std::uint8_t> output;
    std::string error;
    bool ok = false;
    if (ps3)
        ok = SeEditor::Xpac::EncodeZifZigPs3(input, output, error);
    else
        ok = SeEditor::Xpac::EncodeZifZig(input, output, error);

    if (!ok || output.empty())
    {
        _xpacStatus = "Encode failed: " + error;
        return;
    }

    std::filesystem::path outPath = path;
    outPath.replace_extension(zig ? ".zig" : ".zif");
    if (!WriteFileBytes(outPath, output))
    {
        _xpacStatus = "Failed to write " + outPath.string();
        return;
    }

    _xpacStatus = "Encoded " + path.filename().string() + " -> " + outPath.filename().string();
}

void CharmyBee::RenderStuffWindow()
{
    if (!_showStuffWindow)
        return;

    _stuffHeaderMs = 0.0f;
    _stuffTreeMs = 0.0f;
    _stuffSifMs = 0.0f;
    _stuffPopupMs = 0.0f;

    constexpr float kStuffWidth = 320.0f;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 workPos = viewport ? viewport->WorkPos : ImVec2(0.0f, 0.0f);
    ImVec2 workSize = viewport ? viewport->WorkSize : ImVec2(1280.0f, 720.0f);
    float windowHeight = workSize.y - 20.0f;
    if (windowHeight < 200.0f)
        windowHeight = 200.0f;
    ImGui::SetNextWindowPos(ImVec2(workPos.x + 10.0f, workPos.y + 10.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(kStuffWidth, windowHeight), ImGuiCond_Always);
    if (!ImGui::Begin("CppSLib Stuff", &_showStuffWindow))
    {
        ImGui::End();
        return;
    }

    std::filesystem::path root = GetStuffRoot();
    auto headerStart = std::chrono::steady_clock::now();
    ImGui::TextWrapped("Root: %s", root.string().c_str());
    if (ImGui::Button("Change Root..."))
    {
        auto selected = SeEditor::Dialogs::TinyFileDialog::selectFolderDialog("Select CppSLib Stuff Root",
                                                                              root.string());
        if (selected && !selected->empty())
        {
            _stuffRootOverride = std::filesystem::path(*selected);
            std::error_code ec;
            std::filesystem::create_directories(*_stuffRootOverride, ec);
            _xpacStatus = "Stuff root changed.";
            _stuffSifCacheDirty = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh SIF list"))
    {
        _stuffSifCacheDirty = true;
        BuildStuffSifCache(root);
    }
    if (!_xpacStatus.empty())
        ImGui::TextWrapped("%s", _xpacStatus.c_str());
    if (ImGui::Button("Unpack XPAC..."))
        UnpackXpac();
    if (ImGui::Button("Repack XPAC..."))
        RepackXpac();
    if (ImGui::Button("Nuke Stuff Folder"))
        _confirmNukeStuff = true;
    auto headerEnd = std::chrono::steady_clock::now();
    _stuffHeaderMs = std::chrono::duration<float, std::milli>(headerEnd - headerStart).count();

    ImGui::Separator();
    ImGui::BeginChild("StuffTree", ImVec2(0.0f, 0.0f), true);
    auto treeStart = std::chrono::steady_clock::now();
    RenderStuffTreeNode(root);
    auto treeEnd = std::chrono::steady_clock::now();
    _stuffTreeMs = std::chrono::duration<float, std::milli>(treeEnd - treeStart).count();

    auto sifStart = std::chrono::steady_clock::now();
    RenderStuffSifVirtualTree(root);
    auto sifEnd = std::chrono::steady_clock::now();
    _stuffSifMs = std::chrono::duration<float, std::milli>(sifEnd - sifStart).count();
    ImGui::EndChild();

    ImGui::End();

    auto popupStart = std::chrono::steady_clock::now();
    if (_showXpacRepackPopup)
        ImGui::OpenPopup("Select SIFs to Repack");
    if (ImGui::BeginPopupModal("Select SIFs to Repack", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (_xpacRepackEntries.empty())
            ImGui::TextDisabled("No export SIFs found (repack will use unpacked data only).");
        else
        {
            if (ImGui::Button("Select All"))
            {
                for (auto& entry : _xpacRepackEntries)
                    entry.Selected = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear"))
            {
                for (auto& entry : _xpacRepackEntries)
                    entry.Selected = false;
            }

            ImGui::BeginChild("RepackList", ImVec2(420.0f, 300.0f), true);
            for (auto& entry : _xpacRepackEntries)
                ImGui::Checkbox(entry.Label.c_str(), &entry.Selected);
            ImGui::EndChild();
        }

        if (ImGui::Button("Repack"))
        {
            _showXpacRepackPopup = false;
            std::unordered_map<std::string, std::filesystem::path> selectedByName;
            for (auto const& entry : _xpacRepackEntries)
            {
                if (!entry.Selected)
                    continue;
                std::string key = entry.Path.filename().string();
                selectedByName[key] = entry.Path;
            }

            if (_xpacWorker && _xpacWorker->joinable())
                _xpacWorker->join();

            _xpacRepackBusy = true;
            _xpacRepackPopupText = "Repacking " + _lastXpacPath.filename().string();

            auto xpacPath = _lastXpacPath;
            auto repackRoot = _xpacRepackRoot;
            auto exportRoot = GetStuffRoot() / "export";
            _xpacWorker = std::make_unique<std::thread>([this, xpacPath, repackRoot, selectedByName, exportRoot]() {
                std::filesystem::path outPath = exportRoot /
                    (xpacPath.stem().string() + "_repack.xpac");

                Xpac::XpacRepackOptions options;
                options.XpacPath = xpacPath;
                options.InputRoot = repackRoot;
                options.OutputPath = outPath;
                options.ReplacementRoot = exportRoot;
                options.MappingPath = Xpac::FindDefaultMappingPath(options.XpacPath, options.InputRoot);
                options.Progress = [this](std::size_t current, std::size_t total) {
                    _xpacRepackProgress = current;
                    _xpacRepackTotal = total;
                };
                for (auto const& pair : selectedByName)
                {
                    std::filesystem::path src = pair.second;
                    std::filesystem::path rel = src.filename();
                    options.SelectedSifRelativePaths.push_back(rel);
                    if (SeStricmp(src.extension().string().c_str(), ".sif") == 0)
                    {
                        std::filesystem::path sigPath = src;
                        sigPath.replace_extension(".sig");
                        if (std::filesystem::exists(sigPath))
                            options.SelectedSifRelativePaths.push_back(sigPath.filename());
                    }
                }

                Xpac::XpacRepackResult repackResult = Xpac::RepackXpac(options);
                std::ostringstream status;
                status << "[XPAC] Repacked: " << repackResult.RepackedEntries
                       << " / " << repackResult.TotalEntries;
                if (!repackResult.Errors.empty())
                    status << " | Errors: " << repackResult.Errors.size();
                {
                    std::lock_guard<std::mutex> lock(_xpacMutex);
                    _xpacStatus = status.str();
                }
                std::cout << status.str() << std::endl;
                for (auto const& err : repackResult.Errors)
                    std::cout << "[XPAC] " << err << std::endl;

                _stuffSifCacheDirty = true;
                _xpacRepackBusy = false;
            });

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            _showXpacRepackPopup = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    if (_xpacBusy)
        ImGui::OpenPopup("XPAC Unpack");
    if (ImGui::BeginPopupModal("XPAC Unpack", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        std::string text;
        {
            std::lock_guard<std::mutex> lock(_xpacMutex);
            text = _xpacPopupText;
        }
        if (!text.empty())
            ImGui::TextWrapped("%s", text.c_str());

        std::size_t total = _xpacTotal.load();
        std::size_t current = _xpacProgress.load();
        float progress = 0.0f;
        if (total > 0)
            progress = static_cast<float>(current) / static_cast<float>(total);
        ImGui::ProgressBar(progress, ImVec2(320.0f, 0.0f));
        ImGui::Text("Files: %zu / %zu", current, total);

        std::size_t convertTotal = _xpacConvertTotal.load();
        if (convertTotal > 0)
        {
            std::size_t convertCurrent = _xpacConvertProgress.load();
            float convertProgress = static_cast<float>(convertCurrent) / static_cast<float>(convertTotal);
            ImGui::ProgressBar(convertProgress, ImVec2(320.0f, 0.0f));
            ImGui::Text("Decompress: %zu / %zu", convertCurrent, convertTotal);
        }

        if (!_xpacBusy)
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }

    if (_xpacRepackBusy)
        ImGui::OpenPopup("XPAC Repack");
    if (ImGui::BeginPopupModal("XPAC Repack", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!_xpacRepackPopupText.empty())
            ImGui::TextWrapped("%s", _xpacRepackPopupText.c_str());
        std::size_t total = _xpacRepackTotal.load();
        std::size_t current = _xpacRepackProgress.load();
        float progress = 0.0f;
        if (total > 0)
            progress = static_cast<float>(current) / static_cast<float>(total);
        ImGui::ProgressBar(progress, ImVec2(320.0f, 0.0f));
        ImGui::Text("Repack: %zu / %zu", current, total);
        if (!_xpacRepackBusy)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    if (_confirmNukeStuff)
        ImGui::OpenPopup("Confirm Nuke");
    if (ImGui::BeginPopupModal("Confirm Nuke", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextWrapped("Delete everything inside CppSLib_Stuff?");
        ImGui::TextWrapped("This cannot be undone.");
        ImGui::Separator();
        if (ImGui::Button("Yes, delete"))
        {
            if (_xpacWorker && _xpacWorker->joinable())
                _xpacWorker->join();
            std::error_code ec;
            std::filesystem::remove_all(root, ec);
            std::filesystem::create_directories(root, ec);
            _xpacStatus = "CppSLib_Stuff cleared.";
            _confirmNukeStuff = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel"))
        {
            _confirmNukeStuff = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    auto popupEnd = std::chrono::steady_clock::now();
    _stuffPopupMs = std::chrono::duration<float, std::milli>(popupEnd - popupStart).count();
}

void CharmyBee::UnpackXpac()
{
    using namespace SeEditor::Dialogs;
    FileDialogOptions options;
    options.Title = "Unpack XPAC";
    options.FilterPatterns = {"*.xpac"};
    options.FilterDescription = "XPAC files";
    if (!_lastXpacPath.empty())
        options.DefaultPathAndFile = _lastXpacPath.string();

    auto result = TinyFileDialog::openFileDialog(options);
    if (!result)
        return;

    if (_xpacWorker && _xpacWorker->joinable())
        _xpacWorker->join();

    _lastXpacPath = *result;

    _xpacProgress = 0;
    _xpacTotal = 0;
    _xpacConvertProgress = 0;
    _xpacConvertTotal = 0;
    _xpacBusy = true;
    {
        std::lock_guard<std::mutex> lock(_xpacMutex);
        _xpacPopupText = "Unpacking " + _lastXpacPath.filename().string();
    }

    _xpacWorker = std::make_unique<std::thread>([this, path = *result]() {
        Xpac::XpacUnpackOptions unpackOptions;
        unpackOptions.XpacPath = path;
        unpackOptions.OutputRoot = GetStuffRoot();
        unpackOptions.MappingPath = Xpac::FindDefaultMappingPath(unpackOptions.XpacPath, unpackOptions.OutputRoot);
        unpackOptions.ConvertToSifSig = true;
        unpackOptions.Progress = [this](std::size_t current, std::size_t total) {
            _xpacProgress = current;
            _xpacTotal = total;
        };
        unpackOptions.ProgressConvert = [this](std::size_t current, std::size_t total) {
            _xpacConvertProgress = current;
            _xpacConvertTotal = total;
        };

        Xpac::XpacUnpackResult unpackResult = Xpac::UnpackXpac(unpackOptions);
        std::ostringstream status;
        status << "[XPAC] Entries: " << unpackResult.TotalEntries
               << " | ZIF: " << unpackResult.ExtractedZif
               << " | ZIG: " << unpackResult.ExtractedZig
               << " | Converted: " << unpackResult.ConvertedPairs
               << " | Skipped: " << unpackResult.SkippedEntries;
        if (!unpackResult.Errors.empty())
            status << " | Errors: " << unpackResult.Errors.size();

        {
            std::lock_guard<std::mutex> lock(_xpacMutex);
            _xpacStatus = status.str();
        }
        std::cout << status.str() << std::endl;
        for (auto const& err : unpackResult.Errors)
            std::cout << "[XPAC] " << err << std::endl;

        _stuffSifCacheDirty = true;
        _xpacBusy = false;
    });
}

void CharmyBee::RepackXpac()
{
    using namespace SeEditor::Dialogs;
    if (_lastXpacPath.empty())
    {
        FileDialogOptions options;
        options.Title = "Repack XPAC";
        options.FilterPatterns = {"*.xpac"};
        options.FilterDescription = "XPAC files";
        auto result = TinyFileDialog::openFileDialog(options);
        if (!result)
            return;
        _lastXpacPath = *result;
    }

    std::filesystem::path xpacRoot = GetStuffRoot() / "xpac" / _lastXpacPath.stem();
    std::filesystem::path repackRoot = xpacRoot;
    if (auto selected = SeEditor::Dialogs::TinyFileDialog::selectFolderDialog(
            "Select unpacked XPAC folder", repackRoot.string()))
    {
        if (!selected->empty())
            repackRoot = *selected;
    }

    if (!std::filesystem::exists(repackRoot))
    {
        _xpacStatus = "Selected unpacked XPAC folder does not exist.";
        return;
    }

    _xpacRepackRoot = repackRoot;
    _xpacRepackEntries.clear();

    std::error_code ec;
    std::filesystem::path exportRoot = GetStuffRoot() / "export";
    if (!std::filesystem::exists(exportRoot))
    {
        _xpacStatus = "Export folder missing.";
        return;
    }

    for (auto const& entry : std::filesystem::recursive_directory_iterator(exportRoot, ec))
    {
        if (ec)
            break;
        if (!entry.is_regular_file())
            continue;
        if (SeStricmp(entry.path().extension().string().c_str(), ".sif") != 0)
            continue;

        std::filesystem::path relPath = std::filesystem::relative(entry.path(), exportRoot, ec);
        std::vector<std::string> parts;
        for (auto const& part : relPath)
        {
            std::string seg = part.string();
            if (seg.empty())
                continue;
            if (SeStricmp(seg.c_str(), "_Resource") == 0)
                continue;
            if (!seg.empty() && seg[0] == '_')
                seg.erase(seg.begin());
            if (!seg.empty())
                parts.push_back(seg);
        }
        std::string label;
        if (parts.size() <= 1)
        {
            label = parts.empty() ? entry.path().filename().string() : parts.back();
        }
        else
        {
            for (std::size_t i = 1; i < parts.size(); ++i)
            {
                if (i > 1)
                    label += "/";
                label += parts[i];
            }
        }
        if (label.empty())
            label = entry.path().filename().string();

        _xpacRepackEntries.push_back({entry.path(), label, false});
    }

    if (_xpacRepackEntries.empty())
        _xpacStatus = "No SIF files found for repack.";

    _showXpacRepackPopup = true;
}

void CharmyBee::OpenSifFile()
{
    using namespace SeEditor::Dialogs;
    FileDialogOptions options;
    options.Title = "Load SIF/ZIF file";
    options.DefaultPathAndFile = _sifFilePath;
    options.FilterPatterns = {"*.sif", "*.zif", "*.sig", "*.zig"};
    options.FilterDescription = "SIF/ZIF/SIG/ZIG files";

    if (auto result = TinyFileDialog::openFileDialog(options))
    {
        LoadSifFile(*result);
    }
}

void CharmyBee::LoadSifFile(std::filesystem::path const& path)
{
    std::ifstream file(path, std::ios::binary);
    if (!file)
    {
        _sifHexDump.clear();
        _sifFilePath.clear();
        _sifLoadMessage = "Failed to open file.";
        ReportSifError(_sifLoadMessage);
        return;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
    std::vector<std::uint8_t> data(buffer.begin(), buffer.end());
    if (data.empty())
    {
        std::filesystem::path fallback = path;
        if (SeStricmp(path.extension().string().c_str(), ".sif") == 0)
            fallback.replace_extension(".zif");
        else if (SeStricmp(path.extension().string().c_str(), ".sig") == 0)
            fallback.replace_extension(".zig");

        if (fallback != path && std::filesystem::exists(fallback))
        {
            std::ifstream fallbackFile(fallback, std::ios::binary);
            if (fallbackFile)
            {
                std::vector<char> fallbackBuffer((std::istreambuf_iterator<char>(fallbackFile)), {});
                std::vector<std::uint8_t> fallbackData(fallbackBuffer.begin(), fallbackBuffer.end());
                if (!fallbackData.empty())
                {
                    if (LooksLikeZlib(fallbackData))
                        fallbackData = DecompressZlib(fallbackData);
                    StripLengthPrefixIfPresent(fallbackData);
                    data = std::move(fallbackData);
                }
            }
        }
    }

    _sifHexDump = FormatHexDump(data);
    _sifFilePath = path.string();
    _sifOriginalSize = data.size();
    _sifDecompressedSize = data.size();
    _sifFileCompressed = false;
    _sifChunks.clear();
    _sifGpuChunks.clear();
    _sifGpuRaw.clear();
    _sifParseError.clear();

    std::string parseError;
    std::optional<SifParseResult> parseResult = ParseSifFile(data, parseError);
    if (!parseResult)
    {
        _sifParseError = parseError;
        _sifLoadMessage = "Loaded " + std::to_string(data.size()) + " bytes, parse error: " + parseError;
        ReportSifError(_sifLoadMessage);
    }
    else
    {
        _sifChunks = std::move(parseResult->Chunks);
        _sifFileCompressed = parseResult->WasCompressed;
        _sifDecompressedSize = parseResult->DecompressedSize;
        _selectedChunk = !_sifChunks.empty() ? 0 : -1;

        _sifLoadMessage = "Loaded " + std::to_string(data.size()) + " bytes";
        if (_sifFileCompressed)
            _sifLoadMessage += " (decompressed to " + std::to_string(_sifDecompressedSize) + " bytes)";
        if (!_sifChunks.empty())
            _sifLoadMessage += ", parsed " + std::to_string(_sifChunks.size()) + " chunks.";

        _renderer.SetForestMeshes({});
        _renderer.SetDrawForestMeshes(false);
        _drawForestMeshes = false;
        _forestLibrary.reset();
        _sifNavigation.reset();
        _sifNavigationTool.reset();
        _navigationLineEntries.clear();
        _showNavigationHierarchyWindow = false;
        _drawNavigation = false;
        _sifLogic.reset();
        _drawLogic = false;
        _itemsForestLibrary.reset();
        _itemsForestMeshesByForestTree.clear();
        _itemsForestMeshesByTreeHash.clear();
        _logicLocatorMeshes.clear();
        _logicLocatorHasMesh.clear();

        bool bigEndian = !_sifChunks.empty() && _sifChunks.front().BigEndian;
        try
        {
            if (!bigEndian)
            {
                std::filesystem::path gpuPath = path;
                std::filesystem::path altGpuPath;
                if (gpuPath.extension() == ".sif")
                {
                    gpuPath.replace_extension(".sig");
                    altGpuPath = path;
                    altGpuPath.replace_extension(".zig");
                }
                else if (gpuPath.extension() == ".zif")
                {
                    gpuPath.replace_extension(".zig");
                    altGpuPath = path;
                    altGpuPath.replace_extension(".sig");
                }

                std::filesystem::path chosenGpuPath;
                if (std::filesystem::exists(gpuPath))
                    chosenGpuPath = gpuPath;
                else if (!altGpuPath.empty() && std::filesystem::exists(altGpuPath))
                    chosenGpuPath = altGpuPath;

                if (!chosenGpuPath.empty())
                {
                    std::ifstream gpuFile(chosenGpuPath, std::ios::binary);
                    if (gpuFile)
                    {
                        std::vector<char> gpuBuffer((std::istreambuf_iterator<char>(gpuFile)), {});
                        std::vector<std::uint8_t> gpuData(gpuBuffer.begin(), gpuBuffer.end());
                        bool compressed = false;
                        if (LooksLikeZlib(gpuData))
                        {
                            auto inflated = DecompressZlib(gpuData);
                            if (inflated.size() >= 4)
                                gpuData.assign(inflated.begin() + 4, inflated.end());
                            compressed = true;
                        }

                        if (!compressed)
                            StripLengthPrefixIfPresent(gpuData);

                        _sifGpuRaw.assign(gpuData.begin(), gpuData.end());
                        std::cout << "[CharmyBee] Using raw GPU buffer (" << _sifGpuRaw.size() << " bytes) for "
                                  << chosenGpuPath.string() << '.' << std::endl;
                    }
                }
            }
            else
            {
                std::filesystem::path gpuPath = path;
                std::filesystem::path altGpuPath;
                if (gpuPath.extension() == ".sif")
                {
                    gpuPath.replace_extension(".sig");
                    altGpuPath = path;
                    altGpuPath.replace_extension(".zig");
                }
                else if (gpuPath.extension() == ".zif")
                {
                    gpuPath.replace_extension(".zig");
                    altGpuPath = path;
                    altGpuPath.replace_extension(".sig");
                }

                bool preferCompressedGpu = true;
                auto loadGpuFromPath = [&](std::filesystem::path const& candidate,
                                           std::vector<std::uint8_t>& out) -> bool {
                    if (candidate.empty() || !std::filesystem::exists(candidate))
                        return false;

                    std::ifstream gpuFile(candidate, std::ios::binary);
                    if (!gpuFile)
                        return false;

                    std::vector<char> gpuBuffer((std::istreambuf_iterator<char>(gpuFile)), {});
                    std::vector<std::uint8_t> gpuData(gpuBuffer.begin(), gpuBuffer.end());
                    if (gpuData.empty())
                        return false;

                    bool decoded = false;
                    {
                        std::string decodeError;
                        std::vector<std::uint8_t> decodedGpu;
                        if (SeEditor::Xpac::DecodeZifZig(gpuData, decodedGpu, decodeError) && !decodedGpu.empty())
                        {
                            gpuData.swap(decodedGpu);
                            decoded = true;
                        }
                    }

                    if (!decoded)
                    {
                        if (LooksLikeZlib(gpuData))
                        {
                            auto inflated = DecompressZlib(gpuData);
                            if (inflated.size() >= 4)
                                gpuData.assign(inflated.begin() + 4, inflated.end());
                        }
                        else
                        {
                            StripLengthPrefixIfPresent(gpuData);
                        }
                    }

                    out.swap(gpuData);
                    return true;
                };

                std::vector<std::filesystem::path> candidates;
                if (preferCompressedGpu)
                {
                    if (!altGpuPath.empty())
                        candidates.push_back(altGpuPath);
                    candidates.push_back(gpuPath);
                }
                else
                {
                    candidates.push_back(gpuPath);
                    if (!altGpuPath.empty())
                        candidates.push_back(altGpuPath);
                }

                std::filesystem::path chosenGpuPath;
                std::vector<std::uint8_t> gpuData;
                for (auto const& candidate : candidates)
                {
                    if (candidate.empty() || !std::filesystem::exists(candidate))
                        continue;
                    std::vector<std::uint8_t> attempt;
                    if (!loadGpuFromPath(candidate, attempt))
                        continue;
                    if (!attempt.empty())
                    {
                        chosenGpuPath = candidate;
                        gpuData.swap(attempt);
                        break;
                    }
                }

                if (!chosenGpuPath.empty())
                {
                    _sifGpuRaw.assign(gpuData.begin(), gpuData.end());
                    std::cout << "[CharmyBee] Using raw GPU buffer (" << _sifGpuRaw.size() << " bytes) for "
                              << chosenGpuPath.string() << '.' << std::endl;
                }
            }
        }
        catch (...) {}

        LoadCollisionDebugGeometry();
    }
}

void CharmyBee::LoadCollisionDebugGeometry()
{
    _collisionVertices.clear();
    _collisionTriangles.clear();
    _renderer.SetCollisionMesh({}, {});
    _renderer.SetForestBoxes({});
    _forestBoxLayers.clear();
    _allForestMeshes.clear();
    UpdateForestMeshRendering();
    _renderer.SetDrawForestBoxes(false);
    _showForestHierarchyWindow = false;

    auto it = std::find_if(_sifChunks.begin(), _sifChunks.end(),
        [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('C', 'O', 'L', 'I'); });

    if (it == _sifChunks.end())
        return;

    std::string error;
    if (ParseCollisionMeshChunk(*it, _collisionVertices, _collisionTriangles, error))
    {
        _renderer.SetCollisionMesh(_collisionVertices, _collisionTriangles);
        if (!_collisionVertices.empty())
        {
            SlLib::Math::Vector3 min = _collisionVertices[0];
            SlLib::Math::Vector3 max = _collisionVertices[0];
            for (auto const& v : _collisionVertices)
            {
                min.X = std::min(min.X, v.X);
                min.Y = std::min(min.Y, v.Y);
                min.Z = std::min(min.Z, v.Z);
                max.X = std::max(max.X, v.X);
                max.Y = std::max(max.Y, v.Y);
                max.Z = std::max(max.Z, v.Z);
            }
            _collisionCenter = {(min.X + max.X) * 0.5f, (min.Y + max.Y) * 0.5f, (min.Z + max.Z) * 0.5f};
        }
        _orbitTarget = _collisionCenter;
        _orbitOffset = {0.0f, 0.0f, 0.0f};
        std::cout << "[CharmyBee] Collision mesh loaded: " << _collisionVertices.size()
                  << " vertices, " << _collisionTriangles.size() << " tris." << std::endl;
    }
    else
    {
        std::cerr << "[CharmyBee] Collision parse failed: " << error << std::endl;
    }
}

namespace {

struct BoxState
{
    bool Has = false;
    SlLib::Math::Vector3 Min{};
    SlLib::Math::Vector3 Max{};

    void Include(SlLib::Math::Vector3 const& point)
    {
        if (!Has)
        {
            Min = Max = point;
            Has = true;
            return;
        }

        Min.X = std::min(Min.X, point.X);
        Min.Y = std::min(Min.Y, point.Y);
        Min.Z = std::min(Min.Z, point.Z);
        Max.X = std::max(Max.X, point.X);
        Max.Y = std::max(Max.Y, point.Y);
        Max.Z = std::max(Max.Z, point.Z);
    }
};

} // namespace

void CharmyBee::LoadForestDebugGeometry()
{
    LoadForestResources();
}

void CharmyBee::LoadForestResources()
{
    auto log = [&](std::string const& msg) {
        static std::ofstream s_forestLog;
        static bool s_inited = false;
        if (!s_inited)
        {
            s_inited = true;
            try
            {
                std::filesystem::path outPath = std::filesystem::current_path() / "ForestPS3.log";
                // New log per program run: truncate on first use.
                s_forestLog.open(outPath, std::ios::out | std::ios::trunc);
                if (s_forestLog)
                    s_forestLog << "\n=== ForestPS3 session ===\n";
                std::cout << "[ForestPS3] Logging to " << outPath.string() << std::endl;
            }
            catch (...)
            {
                // ignore
            }
        }

        if (s_forestLog)
        {
            s_forestLog << "[ForestPS3] " << msg << "\n";
            s_forestLog.flush();
        }
    };

    _renderer.SetForestMeshes({});
    _renderer.SetDrawForestMeshes(false);
    _drawForestMeshes = false;
    _forestLibrary.reset();
    _allForestMeshes.clear();
    _forestMeshSources.clear();

    auto it = std::find_if(_sifChunks.begin(), _sifChunks.end(),
        [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'); });

    if (it == _sifChunks.end())
    {
        log("No FORE chunk present in loaded SIF.");
        return;
    }

    auto const& chunk = *it;
    std::span<const std::uint8_t> cpuData(chunk.Data.data(), chunk.Data.size());
    std::span<const std::uint8_t> gpuData;
    if (!_sifGpuRaw.empty())
        gpuData = std::span<const std::uint8_t>(_sifGpuRaw.data(), _sifGpuRaw.size());
    else
    {
        auto gpuIt = std::find_if(_sifGpuChunks.begin(), _sifGpuChunks.end(),
            [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'); });
        if (gpuIt != _sifGpuChunks.end() && !gpuIt->Data.empty())
            gpuData = std::span<const std::uint8_t>(gpuIt->Data.data(), gpuIt->Data.size());
    }

    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_ps3("ps3", true, false, 0);
    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(chunk.Relocations.size());
    for (auto offset : chunk.Relocations)
        relocations.push_back({static_cast<int>(offset), 0});
    SlLib::Serialization::ResourceLoadContext context(cpuData, gpuData, std::move(relocations));
    bool isBigEndian = chunk.BigEndian;
    context.Platform = isBigEndian ? &s_ps3 : &s_win32;

    if (isBigEndian)
    {
        log("FORE chunk is big-endian (PS3). cpuBytes=" + std::to_string(cpuData.size()) +
            " gpuBytes=" + std::to_string(gpuData.size()) +
            " relocCount=" + std::to_string(chunk.Relocations.size()));
        // Keep console output compact; detailed trace goes to ForestPS3.log
        std::cout << "[ForestPS3] PS3 FORE load started (details in ForestPS3.log)\n";

        auto readU32BE = [&](std::size_t off) -> std::uint32_t {
            if (off + 4 > cpuData.size())
                return 0;
            return (static_cast<std::uint32_t>(cpuData[off]) << 24) |
                   (static_cast<std::uint32_t>(cpuData[off + 1]) << 16) |
                   (static_cast<std::uint32_t>(cpuData[off + 2]) << 8) |
                   static_cast<std::uint32_t>(cpuData[off + 3]);
        };
        auto hex8 = [](std::uint32_t v) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%08X", v);
            return std::string(buf);
        };

        struct PrimitiveHitRaw
        {
            std::uint32_t PrimitiveStart = 0;
            std::uint32_t Sig = 0;
            std::uint32_t NumIndices = 0;
            std::uint32_t IndexPtrRaw = 0;
            std::uint32_t VbBasePtrRaw = 0;
            std::uint32_t StreamSlot = 0;
        };
        std::vector<PrimitiveHitRaw> primitiveHitsRaw;
        primitiveHitsRaw.reserve(512);

        if (cpuData.size() >= 0x10)
        {
            std::uint32_t h0 = readU32BE(0x00);
            std::uint32_t h1 = readU32BE(0x04);
            std::uint32_t h2 = readU32BE(0x08);
            std::uint32_t rootOffsetHdr = readU32BE(0x0C);
            log("ForestHeader: one=0x" + hex8(h0) + " id=0x" + hex8(h1) + " sizeMinus12=0x" + hex8(h2) +
                " rootOffset=0x" + hex8(rootOffsetHdr));

            if (rootOffsetHdr + 0x34 <= cpuData.size())
            {
                std::uint32_t sectionCount = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x00);
                std::uint32_t rootStructBytes = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x04);
                std::uint32_t sectionTableOff = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x0C);
                std::uint32_t hashTableOff = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x14);
                std::uint32_t nameRegistryPtr = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x24);
                std::uint32_t relocCountRoot = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x28);
                std::uint32_t relocPtrRoot = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x2C);
                std::uint32_t libraryPtr = readU32BE(static_cast<std::size_t>(rootOffsetHdr) + 0x30);
                log("Root@0x" + hex8(rootOffsetHdr) + ": sectionCount=" + std::to_string(sectionCount) +
                    " rootBytes=0x" + hex8(rootStructBytes) +
                    " sectionTableOff=0x" + hex8(sectionTableOff) +
                    " hashTableOff=0x" + hex8(hashTableOff) +
                    " nameRegistryPtr=0x" + hex8(nameRegistryPtr) +
                    " relocCount=0x" + hex8(relocCountRoot) +
                    " relocPtr=0x" + hex8(relocPtrRoot) +
                    " libraryPtr=0x" + hex8(libraryPtr));
                bool relocRangeOk = false;
                if (relocCountRoot > 0 && relocCountRoot < 2'000'000 && relocPtrRoot > 0 && (relocPtrRoot % 4) == 0)
                {
                    std::uint64_t end = static_cast<std::uint64_t>(relocPtrRoot) + static_cast<std::uint64_t>(relocCountRoot) * 4ull;
                    relocRangeOk = (end <= cpuData.size());
                }
                log(std::string("Root relocation range ok=") + (relocRangeOk ? "true" : "false"));

                // Dump section table (first 16 entries) for deterministic reverse-engineering.
                if (sectionTableOff != 0 && sectionCount > 0 &&
                    static_cast<std::size_t>(sectionTableOff) + static_cast<std::size_t>(sectionCount) * 4 <= cpuData.size())
                {
                    std::string s = "SectionTable[0..min(15)] @0x" + hex8(sectionTableOff) + ":";
                    std::uint32_t lim = std::min<std::uint32_t>(sectionCount, 16);
                    for (std::uint32_t i = 0; i < lim; ++i)
                    {
                        std::uint32_t off = readU32BE(static_cast<std::size_t>(sectionTableOff) + static_cast<std::size_t>(i) * 4);
                        s += " " + hex8(off);
                    }
                    log(s);
                }

                // Dump first bytes of library block (deterministic, no scanning).
                if (libraryPtr != 0 && static_cast<std::size_t>(libraryPtr) + 0x100 <= cpuData.size())
                {
                    std::string s = "Library u32[0..15] @0x" + hex8(libraryPtr) + ":";
                    for (int i = 0; i < 16; ++i)
                    {
                        std::uint32_t v = readU32BE(static_cast<std::size_t>(libraryPtr) + static_cast<std::size_t>(i) * 4);
                        s += " " + hex8(v);
                    }
                    log(s);
                }

                // Deterministic graph walk from known root pointers (no file-wide scanning).
                // Safeguards:
                //  - bounded node count
                //  - bounded words inspected per node
                //  - strict pointer plausibility checks
                {
                    // NOTE: PS3 track forests require walking deeper than racer forests.
                    // Keep the walk bounded, but large enough to reach primitives/streams.
                    constexpr std::size_t kMaxNodes = 4096;
                    constexpr std::size_t kWordsPerNode = 48; // 192 bytes
                    constexpr std::size_t kMaxPrimitiveHits = 512;
                    constexpr std::size_t kMaxNodeDumps = 256;

                    // PS3 pointers can be raw offsets (cpu) or tagged VAs:
                    //   CPU: 0x0020_0000 + offset
                    //   GPU: 0x0180_0000 + offset
                    struct Ps3Ptr
                    {
                        std::uint32_t Raw = 0;
                        std::uint32_t Offset = 0;
                        bool IsGpu = false;
                        bool Valid = false;
                    };
                    auto decodePs3PtrLocal = [&](std::uint32_t raw) -> Ps3Ptr {
                        Ps3Ptr out{};
                        out.Raw = raw;
                        if (raw == 0)
                            return out;

                        // Untagged offsets.
                        if (raw < cpuData.size())
                        {
                            out.Offset = raw;
                            out.IsGpu = false;
                            out.Valid = true;
                            return out;
                        }

                        // Tagged bases.
                        constexpr std::uint32_t kCpuBase = 0x00200000u;
                        constexpr std::uint32_t kGpuBase = 0x01800000u;
                        if (raw >= kCpuBase)
                        {
                            std::uint32_t off = raw - kCpuBase;
                            if (off < cpuData.size())
                            {
                                out.Offset = off;
                                out.IsGpu = false;
                                out.Valid = true;
                                return out;
                            }
                        }
                        if (raw >= kGpuBase)
                        {
                            std::uint32_t off = raw - kGpuBase;
                            if (off < gpuData.size())
                            {
                                out.Offset = off;
                                out.IsGpu = true;
                                out.Valid = true;
                                return out;
                            }
                        }

                        return out;
                    };

                    auto isPlausibleCpuPtr = [&](std::uint32_t p) -> bool {
                        return p >= 0x20 && (p % 4) == 0 && static_cast<std::size_t>(p) + 4 <= cpuData.size();
                    };

                    std::vector<std::uint32_t> queue;
                    queue.reserve(kMaxNodes);
                    std::unordered_set<std::uint32_t> visited;
                    visited.reserve(kMaxNodes * 2);

                    auto enqueue = [&](std::uint32_t p, const char* reason) {
                        if (!isPlausibleCpuPtr(p))
                            return;
                        if (visited.find(p) != visited.end())
                            return;
                        if (visited.size() >= kMaxNodes)
                            return;
                        visited.insert(p);
                        queue.push_back(p);
                        log(std::string("enqueue ") + reason + " @0x" + hex8(p));
                    };

                    auto enqueueFromRaw = [&](std::uint32_t raw, const char* reason) {
                        Ps3Ptr p = decodePs3PtrLocal(raw);
                        if (!p.Valid || p.IsGpu)
                            return;
                        enqueue(p.Offset, reason);
                    };

                    // Seed pointers: library/name registry/hash table + any section entries that are in-range.
                    enqueue(libraryPtr, "libraryPtr");
                    enqueue(nameRegistryPtr, "nameRegistryPtr");
                    enqueue(hashTableOff, "hashTableOff");

                    if (sectionTableOff != 0 && sectionCount > 0 &&
                        static_cast<std::size_t>(sectionTableOff) + static_cast<std::size_t>(sectionCount) * 4 <= cpuData.size())
                    {
                        for (std::uint32_t i = 0; i < sectionCount; ++i)
                        {
                            std::uint32_t off = readU32BE(static_cast<std::size_t>(sectionTableOff) + static_cast<std::size_t>(i) * 4);
                            // Section table contents are not reliably "pure offsets" on PS3 tracks, but may contain
                            // tagged pointers. Decode and enqueue only plausible CPU pointers.
                            enqueueFromRaw(off, "sectionEntry");
                        }
                    }

                    std::size_t walked = 0;
                    while (!queue.empty() && walked < kMaxNodes)
                    {
                        std::uint32_t addr = queue.back();
                        queue.pop_back();
                        walked++;

                        // Dump a small window of u32s so we can see structure signatures.
                        std::string dump = "node @0x" + hex8(addr) + " u32:";
                        std::size_t maxWords = std::min<std::size_t>(kWordsPerNode,
                            (cpuData.size() - static_cast<std::size_t>(addr)) / 4);
                        for (std::size_t wi = 0; wi < std::min<std::size_t>(16, maxWords); ++wi)
                        {
                            std::uint32_t v = readU32BE(static_cast<std::size_t>(addr) + wi * 4);
                            dump += " " + hex8(v);
                        }
                        if (walked <= kMaxNodeDumps)
                            log(dump);

                        // Primitive detector (deterministic local check; PS3 track variant).
                        // We do NOT require knowing the full struct type here. We only extract the proven tail fields:
                        //  - numIndices @ +0x90 (BE u32)
                        //  - indexPtrRaw @ +0x94 (tag/off24)
                        //  - vbBasePtrRaw @ +0x18 (tag=0x04, off24 in BIN/cpuData)
                        //  - streamSlot @ +0x9C (small enum/slot; NOT a pointer)
                        // Additionally we record sig/selfOff if present for later filtering.
                        if (primitiveHitsRaw.size() < kMaxPrimitiveHits && static_cast<std::size_t>(addr) + 0xA8 <= cpuData.size())
                        {
                            // Primitives are often elements inside a struct array and not directly pointer-targeted.
                            // Try a small local search window within this node for an aligned primitive start.
                            for (std::size_t so = 0; so <= 0x80 && primitiveHitsRaw.size() < kMaxPrimitiveHits; so += 4)
                            {
                                std::uint32_t cand = addr + static_cast<std::uint32_t>(so);
                                if (static_cast<std::size_t>(cand) + 0xA8 > cpuData.size())
                                    break;

                                std::uint32_t sig = readU32BE(static_cast<std::size_t>(cand) + 0x00);
                                std::uint32_t selfOff = readU32BE(static_cast<std::size_t>(cand) + 0x2C);

                                std::uint32_t vbBaseRaw = readU32BE(static_cast<std::size_t>(cand) + 0x18);
                                std::uint32_t vbOff24 = (vbBaseRaw & 0x00FFFFFFu);
                                if (vbOff24 == 0)
                                    continue;

                                std::uint32_t numIndices = readU32BE(static_cast<std::size_t>(cand) + 0x90);
                                if (numIndices < 3 || numIndices > 2'000'000)
                                    continue;

                                std::uint32_t idxRaw = readU32BE(static_cast<std::size_t>(cand) + 0x94);
                                if (idxRaw == 0)
                                    continue;

                                std::uint32_t slot32 = readU32BE(static_cast<std::size_t>(cand) + 0x9C);
                                std::uint8_t slot8 = cpuData[static_cast<std::size_t>(cand) + 0x9F];
                                std::uint32_t slot = (slot32 <= 0xFFu) ? slot32 : static_cast<std::uint32_t>(slot8);

                                PrimitiveHitRaw hit;
                                hit.PrimitiveStart = cand;
                                hit.Sig = sig;
                                hit.NumIndices = numIndices;
                                hit.IndexPtrRaw = idxRaw;
                                hit.VbBasePtrRaw = vbBaseRaw;
                                hit.StreamSlot = slot;
                                primitiveHitsRaw.push_back(hit);
                                log("primitiveHit @0x" + hex8(cand) +
                                    " sig=0x" + hex8(sig) +
                                    (selfOff == cand ? " selfOk" : " selfNo") +
                                    " slot32=0x" + hex8(slot32) +
                                    " slot8=" + std::to_string(static_cast<unsigned>(slot8)) +
                                    " slot=" + std::to_string(slot) +
                                    " numIdx=" + std::to_string(numIndices) +
                                    " vbBaseRaw=0x" + hex8(vbBaseRaw) +
                                    " idxPtrRaw=0x" + hex8(idxRaw));
                            }
                        }

                        // Heuristic-free pointer harvesting: treat any in-range u32 as potential pointer target.
                        // This is bounded and driven strictly by the pointer graph (no global scan).
                        for (std::size_t wi = 0; wi < maxWords; ++wi)
                        {
                            std::uint32_t v = readU32BE(static_cast<std::size_t>(addr) + wi * 4);
                            // Many pointers are tagged. Decode and enqueue CPU pointers only.
                            enqueueFromRaw(v, "field");

                            // Also detect simple array headers (count, ptr) patterns locally.
                            if (wi + 1 < maxWords)
                            {
                                std::uint32_t count = v;
                                std::uint32_t ptr = readU32BE(static_cast<std::size_t>(addr) + (wi + 1) * 4);
                                if (count > 0 && count < 100000)
                                {
                                    Ps3Ptr p = decodePs3PtrLocal(ptr);
                                    if (p.Valid && !p.IsGpu && isPlausibleCpuPtr(p.Offset))
                                    {
                                        log("array? at 0x" + hex8(static_cast<std::uint32_t>(addr + wi * 4)) +
                                            " count=" + std::to_string(count) +
                                            " ptrRaw=0x" + hex8(ptr) +
                                            " ptrOff=0x" + hex8(p.Offset));
                                        enqueue(p.Offset, "arrayPtr");
                                    }
                                }
                            }
                        }
                    }

                    log("Graph walk done: visited=" + std::to_string(visited.size()) +
                        " walked=" + std::to_string(walked));
                    std::cout << "[ForestPS3] Graph walk done: visited=" << visited.size()
                              << " walked=" << walked << " (see ForestPS3.log)\n";

                    log("primitiveHitsRaw=" + std::to_string(primitiveHitsRaw.size()));
                    std::cout << "[ForestPS3] primitiveHits=" << primitiveHitsRaw.size() << " (see ForestPS3.log)\n";
                }
            }
            else
            {
                log("RootOffset points out of range for expected PS3 root fields.");
            }
        }

        // PS3 forest: the PC object graph loader does not match the BE layout yet.
        // Deterministic PS3 rendering path:
        // - Use the SIF relocation table as the only "entry set" (no file-wide scanning).
        // - Find stream headers (Layout-B) and then find primitives via relocation pointers to VertexStream fields.
        // - Render using position-at-offset-0 as float3 (no UV/normal mapping yet), and u16 indices.
        auto looksAscii = [&](std::uint32_t ptr, std::size_t minLen, std::size_t maxLen) -> bool {
            if (ptr == 0 || ptr >= cpuData.size())
                return false;
            std::size_t len = 0;
            for (; len <= maxLen && ptr + len < cpuData.size(); ++len)
            {
                unsigned char c = cpuData[ptr + len];
                if (c == 0)
                    break;
                if (c < 0x20 || c > 0x7E)
                    return false;
            }
            return (len >= minLen && len <= maxLen && ptr + len < cpuData.size() && cpuData[ptr + len] == 0);
        };
        auto readFloatLE = [](std::uint32_t u) -> float {
            float f{};
            std::memcpy(&f, &u, sizeof(float));
            return f;
        };
        auto swapU32 = [](std::uint32_t v) -> std::uint32_t {
            return (v >> 24) | ((v >> 8) & 0x0000FF00u) | ((v << 8) & 0x00FF0000u) | (v << 24);
        };

        // PS3 pointers in track forests can be tagged virtual addresses.
        // Observed bases:
        //  - CPU: 0x0020_0000 + offset
        //  - GPU: 0x0180_0000 + offset
        struct Ps3Ptr
        {
            std::uint32_t Raw = 0;
            std::uint32_t Offset = 0;
            bool IsGpu = false;
            bool Valid = false;
        };
        auto decodePs3Ptr = [&](std::uint32_t raw) -> Ps3Ptr {
            Ps3Ptr out{};
            out.Raw = raw;
            if (raw == 0)
                return out;

            // Untagged offsets.
            if (raw < cpuData.size())
            {
                out.Offset = raw;
                out.IsGpu = false;
                out.Valid = true;
                return out;
            }
            if (raw < gpuData.size())
            {
                // This can happen for small GPU pointers when they are already relative.
                out.Offset = raw;
                out.IsGpu = true;
                out.Valid = true;
                return out;
            }

            // Tagged bases.
            constexpr std::uint32_t kCpuBase = 0x00200000u;
            constexpr std::uint32_t kGpuBase = 0x01800000u;
            if (raw >= kCpuBase)
            {
                std::uint32_t off = raw - kCpuBase;
                if (off < cpuData.size())
                {
                    out.Offset = off;
                    out.IsGpu = false;
                    out.Valid = true;
                    return out;
                }
            }
            if (raw >= kGpuBase)
            {
                std::uint32_t off = raw - kGpuBase;
                if (off < gpuData.size())
                {
                    out.Offset = off;
                    out.IsGpu = true;
                    out.Valid = true;
                    return out;
                }
            }

            return out;
        };

        struct Ps3StreamHeader
        {
            std::uint32_t headerStart = 0;
            std::uint32_t stride = 0;
            std::uint32_t count = 0;
            std::uint32_t vertexDataPtr = 0;
            bool vertexInGpu = false;
        };

        std::vector<Ps3StreamHeader> streams;
        streams.reserve(256);
        std::unordered_map<std::uint32_t, std::size_t> streamIndex;
        streamIndex.reserve(256);

        // Prefer the relocation table embedded in the PS3 Forest data itself (deterministic, no scanning).
        // The SIF chunk relocation list for PS3 tracks can be tiny (e.g. 14), which is insufficient.
        std::vector<SlLib::Resources::Database::SlResourceRelocation> ps3Relocs;
        ps3Relocs.reserve(8192);
        std::vector<std::uint32_t> relocOffsets; // offset-only (field offsets)
        relocOffsets.reserve(8192);
        {
            if (cpuData.size() >= 0x30)
            {
                std::uint32_t rootOffset = readU32BE(0x0C);
                if (rootOffset + 0x30 <= cpuData.size())
                {
                    std::uint32_t relocCount = readU32BE(static_cast<std::size_t>(rootOffset) + 0x28);
                    std::uint32_t relocPtr = readU32BE(static_cast<std::size_t>(rootOffset) + 0x2C);
                    if (relocCount > 0 && relocCount < 2'000'000 && relocPtr > 0 && (relocPtr % 4) == 0)
                    {
                        // Dump a small raw preview so we can verify the actual on-disk format.
                        {
                            std::size_t dumpBytes = std::min<std::size_t>(64, cpuData.size() - static_cast<std::size_t>(relocPtr));
                            std::string hex;
                            hex.reserve(dumpBytes * 3);
                            for (std::size_t i = 0; i < dumpBytes; ++i)
                            {
                                char b[4];
                                std::snprintf(b, sizeof(b), "%02X", cpuData[static_cast<std::size_t>(relocPtr) + i]);
                                hex += b;
                                if (i + 1 < dumpBytes)
                                    hex += ' ';
                            }
                            log("Reloc raw @0x" + hex8(relocPtr) + " (first " + std::to_string(dumpBytes) + "): " + hex);
                        }

                        auto isPlausibleOff = [&](std::uint32_t off) -> bool {
                            return off >= 0x20 && off < cpuData.size() && (off % 4) == 0;
                        };

                        auto readU16BE = [&](std::size_t off) -> std::uint16_t {
                            if (off + 2 > cpuData.size())
                                return 0;
                            return static_cast<std::uint16_t>((cpuData[off] << 8) | cpuData[off + 1]);
                        };
                        auto readU16LE = [&](std::size_t off) -> std::uint16_t {
                            if (off + 2 > cpuData.size())
                                return 0;
                            return static_cast<std::uint16_t>(cpuData[off] | (cpuData[off + 1] << 8));
                        };
                        auto readU32LELocal = [&](std::size_t off) -> std::uint32_t {
                            if (off + 4 > cpuData.size())
                                return 0;
                            return static_cast<std::uint32_t>(cpuData[off]) |
                                   (static_cast<std::uint32_t>(cpuData[off + 1]) << 8) |
                                   (static_cast<std::uint32_t>(cpuData[off + 2]) << 16) |
                                   (static_cast<std::uint32_t>(cpuData[off + 3]) << 24);
                        };

                        struct Candidate
                        {
                            const char* Name = nullptr;
                            std::size_t StrideBytes = 0;
                            bool Little = false;
                            bool U16 = false;
                            std::size_t Plausible = 0;
                            std::size_t NonZero = 0;
                        };
                        Candidate cands[] = {
                            {"u32-be", 4, false, false, 0, 0},
                            {"u32-le", 4, true, false, 0, 0},
                            {"u16-be", 2, false, true, 0, 0},
                            {"u16-le", 2, true, true, 0, 0},
                        };

                        auto score = [&](Candidate& c) {
                            c.Plausible = 0;
                            c.NonZero = 0;
                            std::uint32_t sampleN = std::min<std::uint32_t>(relocCount, 512);
                            std::size_t need = static_cast<std::size_t>(relocPtr) + static_cast<std::size_t>(sampleN) * c.StrideBytes;
                            if (need > cpuData.size())
                                return;
                            for (std::uint32_t i = 0; i < sampleN; ++i)
                            {
                                std::size_t at = static_cast<std::size_t>(relocPtr) + static_cast<std::size_t>(i) * c.StrideBytes;
                                std::uint32_t off = 0;
                                if (c.U16)
                                    off = c.Little ? readU16LE(at) : readU16BE(at);
                                else
                                    off = c.Little ? readU32LELocal(at) : readU32BE(at);
                                if (off != 0)
                                    c.NonZero++;
                                if (isPlausibleOff(off))
                                    c.Plausible++;
                            }
                        };

                        for (auto& c : cands)
                            score(c);

                        Candidate* best = &cands[0];
                        for (auto& c : cands)
                        {
                            if (c.Plausible > best->Plausible)
                                best = &c;
                        }

                        log(std::string("Reloc decode candidates: ") +
                            "u32-be(p=" + std::to_string(cands[0].Plausible) + ",nz=" + std::to_string(cands[0].NonZero) + ") " +
                            "u32-le(p=" + std::to_string(cands[1].Plausible) + ",nz=" + std::to_string(cands[1].NonZero) + ") " +
                            "u16-be(p=" + std::to_string(cands[2].Plausible) + ",nz=" + std::to_string(cands[2].NonZero) + ") " +
                            "u16-le(p=" + std::to_string(cands[3].Plausible) + ",nz=" + std::to_string(cands[3].NonZero) + ")");
                        log(std::string("Selected reloc decode: ") + best->Name + " (plausible=" + std::to_string(best->Plausible) + ")");

                        // Decode using the selected format.
                        relocOffsets.clear();
                        relocOffsets.reserve(relocCount);
                        for (std::uint32_t i = 0; i < relocCount; ++i)
                        {
                            std::size_t at = static_cast<std::size_t>(relocPtr) + static_cast<std::size_t>(i) * best->StrideBytes;
                            if (at + best->StrideBytes > cpuData.size())
                                break;
                            std::uint32_t off = 0;
                            if (best->U16)
                                off = best->Little ? readU16LE(at) : readU16BE(at);
                            else
                                off = best->Little ? readU32LELocal(at) : readU32BE(at);
                            // Filter zero entries (common padding/no-op).
                            if (off != 0)
                                relocOffsets.push_back(off);
                        }
                        log("Using embedded PS3 relocations (" + std::string(best->Name) + "): count=" + std::to_string(relocOffsets.size()) +
                            " rootOffset=0x" + hex8(rootOffset) +
                            " relocPtr=0x" + hex8(relocPtr));
                    }
                }
            }
            if (ps3Relocs.empty() && relocOffsets.empty())
            {
                log("Embedded PS3 relocation table not found/invalid; falling back to SIF relocations.");
                relocOffsets = chunk.Relocations;
            }
        }

        auto tryParseStreamLayoutKindFirst = [&](std::uint32_t addr, Ps3StreamHeader& out) -> bool {
            // Layout (big-endian, deterministic local parse):
            //   +0x00 kind
            //   +0x04 vertexDataPtr
            //   +0x18 numExtra
            //   +0x1C stride
            //   +0x20 count
            //   +0x24 flags
            if (addr + 0x28 > cpuData.size() || (addr % 4) != 0)
                return false;
            std::uint32_t stride = readU32BE(addr + 0x1C);
            std::uint32_t count = readU32BE(addr + 0x20);
            Ps3Ptr vertexPtr = decodePs3Ptr(readU32BE(addr + 0x04));
            if (stride < 12 || stride > 0x400 || (stride % 2) != 0 || count == 0 || count > 5'000'000)
                return false;
            std::uint64_t bytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(count);
            if (!vertexPtr.Valid || vertexPtr.Offset == 0)
                return false;
            bool inGpu = vertexPtr.IsGpu && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= gpuData.size());
            bool inCpu = (!vertexPtr.IsGpu) && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= cpuData.size());
            if (!inGpu && !inCpu)
                return false;
            // Filter obvious false-positives: vertex buffers for tracks won't live in the header region.
            if (inCpu && vertexPtr.Offset < 0x1000)
                return false;
            out.headerStart = addr;
            out.stride = stride;
            out.count = count;
            out.vertexDataPtr = vertexPtr.Offset;
            out.vertexInGpu = inGpu;
            return true;
        };

        auto tryParseStreamLayoutExtraFirst = [&](std::uint32_t addr, Ps3StreamHeader& out) -> bool {
            // Alternate layout (extraPtr first) that we used for racer forests:
            //   +0x0C stride
            //   +0x10 count
            //   vertex data ptr is stored in an owner tail at -0x14 (not always true for tracks)
            if (addr + 0x18 > cpuData.size() || (addr % 4) != 0 || addr < 0x18)
                return false;
            std::uint32_t stride = readU32BE(addr + 0x0C);
            std::uint32_t count = readU32BE(addr + 0x10);
            if (stride < 12 || stride > 0x400 || (stride % 2) != 0 || count == 0 || count > 5'000'000)
                return false;
            Ps3Ptr vertexPtr = decodePs3Ptr(readU32BE(addr - 0x14));
            std::uint64_t bytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(count);
            if (!vertexPtr.Valid || vertexPtr.Offset == 0)
                return false;
            bool inGpu = vertexPtr.IsGpu && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= gpuData.size());
            bool inCpu = (!vertexPtr.IsGpu) && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= cpuData.size());
            if (!inGpu && !inCpu)
                return false;
            if (inCpu && vertexPtr.Offset < 0x1000)
                return false;
            out.headerStart = addr;
            out.stride = stride;
            out.count = count;
            out.vertexDataPtr = vertexPtr.Offset;
            out.vertexInGpu = inGpu;
            return true;
        };

        auto looksAsciiAt = [&](std::uint32_t ptr, std::size_t minLen, std::size_t maxLen) -> bool {
            if (ptr == 0 || ptr >= cpuData.size())
                return false;
            std::size_t len = 0;
            for (; len <= maxLen && static_cast<std::size_t>(ptr) + len < cpuData.size(); ++len)
            {
                unsigned char c = cpuData[static_cast<std::size_t>(ptr) + len];
                if (c == 0)
                    break;
                if (c < 0x20 || c > 0x7E)
                    return false;
            }
            return (len >= minLen && len <= maxLen && static_cast<std::size_t>(ptr) + len < cpuData.size() &&
                    cpuData[static_cast<std::size_t>(ptr) + len] == 0);
        };

        auto tryParseStreamLayoutB = [&](std::uint32_t addr, Ps3StreamHeader& out) -> bool {
            // Layout-B (extraPtr/namePtr/numExtra/stride/count/flags), common on PS3.
            // Vertex data ptr typically lives in an "owner tail" at addr-0x14 (tagged pointer allowed).
            if (addr + 0x18 > cpuData.size() || (addr % 4) != 0 || addr < 0x18)
                return false;
            std::uint32_t extraPtrRaw = readU32BE(addr + 0x00);
            std::uint32_t namePtrRaw = readU32BE(addr + 0x04);
            std::uint32_t stride = readU32BE(addr + 0x0C);
            std::uint32_t count = readU32BE(addr + 0x10);
            if (stride < 12 || stride > 0x400 || (stride % 2) != 0 || count == 0 || count > 5'000'000)
                return false;

            // Name pointer should point into ASCII string table region for this track.
            if (!looksAsciiAt(namePtrRaw, 2, 96))
                return false;

            // Extra pointer typically points to a code list (first u32 <= 0xFF) or is 0.
            if (extraPtrRaw != 0)
            {
                Ps3Ptr extraPtr = decodePs3Ptr(extraPtrRaw);
                if (!extraPtr.Valid || extraPtr.IsGpu || static_cast<std::size_t>(extraPtr.Offset) + 4 > cpuData.size())
                    return false;
                std::uint32_t code0 = readU32BE(extraPtr.Offset);
                if (code0 > 0xFF)
                    return false;
            }

            Ps3Ptr vertexPtr = decodePs3Ptr(readU32BE(addr - 0x14));
            std::uint64_t bytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(count);
            if (!vertexPtr.Valid || vertexPtr.Offset == 0)
                return false;
            bool inGpu = vertexPtr.IsGpu && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= gpuData.size());
            bool inCpu = (!vertexPtr.IsGpu) && (static_cast<std::uint64_t>(vertexPtr.Offset) + bytes <= cpuData.size());
            if (!inGpu && !inCpu)
                return false;
            if (inCpu && vertexPtr.Offset < 0x1000)
                return false;

            out.headerStart = addr;
            out.stride = stride;
            out.count = count;
            out.vertexDataPtr = vertexPtr.Offset;
            out.vertexInGpu = inGpu;
            return true;
        };

        // Seed candidates: every relocation points at a pointer field.
        // We only inspect pointer targets reachable from those pointers (no file-wide scanning).
        std::size_t streamsKindFirst = 0;
        std::size_t streamsExtraFirst = 0;
        std::size_t streamsLayoutB = 0;
        std::size_t relocCpuPtrCount = 0;
        std::size_t relocGpuPtrCount = 0;
        std::size_t relocZeroPtrCount = 0;
        std::size_t relocOtherPtrCount = 0;
        std::size_t loggedRelocs = 0;

        // Pointer field reads (raw), then decode tagged addresses.
        auto readPtrAt = [&](std::uint32_t off, bool& gpu) -> std::uint32_t {
            gpu = false;
            if (off + 4 > cpuData.size())
                return 0;
            std::uint32_t raw = readU32BE(off);
            Ps3Ptr p = decodePs3Ptr(raw);
            gpu = p.Valid && p.IsGpu;
            return p.Valid ? p.Offset : 0;
        };

        auto const& seedRelocs = !ps3Relocs.empty() ? ps3Relocs : [&]() {
            static std::vector<SlLib::Resources::Database::SlResourceRelocation> tmp;
            tmp.clear();
            tmp.reserve(relocOffsets.size());
            for (auto off : relocOffsets)
                tmp.push_back({static_cast<int>(off), 0});
            return tmp;
        }();

        for (auto const& rel : seedRelocs)
        {
            std::uint32_t fieldOff = static_cast<std::uint32_t>(rel.Offset);
            if (fieldOff + 4 > cpuData.size())
                continue;
            bool ptrGpu = false;
            std::uint32_t target = readPtrAt(fieldOff, ptrGpu);
            if (target == 0)
            {
                relocZeroPtrCount++;
                continue;
            }

            bool pointsCpu = (!ptrGpu) && (target < cpuData.size());
            bool pointsGpu = ptrGpu && (target < gpuData.size());
            if (pointsCpu)
                relocCpuPtrCount++;
            else if (pointsGpu)
                relocGpuPtrCount++;
            else
                relocOtherPtrCount++;

            if (loggedRelocs < 20)
            {
                log("reloc[" + std::to_string(loggedRelocs) + "]: fieldOff=0x" + hex8(fieldOff) +
                    " ptr=0x" + hex8(target) +
                    (pointsCpu ? " (cpu)" : (pointsGpu ? " (gpu)" : " (out)")));
                loggedRelocs++;
            }

            if (target < 0x20 || (target % 4) != 0)
                continue;

            if (streamIndex.find(target) != streamIndex.end())
                continue;

            Ps3StreamHeader parsed{};
            if (tryParseStreamLayoutKindFirst(target, parsed))
            {
                streamIndex.emplace(target, streams.size());
                streams.push_back(parsed);
                streamsKindFirst++;
                continue;
            }
            if (tryParseStreamLayoutExtraFirst(target, parsed))
            {
                streamIndex.emplace(target, streams.size());
                streams.push_back(parsed);
                streamsExtraFirst++;
                continue;
            }
            if (tryParseStreamLayoutB(target, parsed))
            {
                streamIndex.emplace(target, streams.size());
                streams.push_back(parsed);
                streamsLayoutB++;
                continue;
            }
        }

        log("Discovered PS3 stream headers: " + std::to_string(streams.size()) +
            " (kindFirst=" + std::to_string(streamsKindFirst) +
            " extraFirst=" + std::to_string(streamsExtraFirst) +
            " layoutB=" + std::to_string(streamsLayoutB) + ")");
        std::cout << "[ForestPS3] streamHeaders=" << streams.size()
                  << " kindFirst=" << streamsKindFirst
                  << " extraFirst=" << streamsExtraFirst
                  << " layoutB=" << streamsLayoutB
                  << " (see ForestPS3.log)\n";
        log("Reloc ptr distribution: cpu=" + std::to_string(relocCpuPtrCount) +
            " gpu=" + std::to_string(relocGpuPtrCount) +
            " zero=" + std::to_string(relocZeroPtrCount) +
            " out=" + std::to_string(relocOtherPtrCount) +
            " total=" + std::to_string(relocOffsets.size()));
        // Deterministic PS3 track path (BillyHatcher_Hard proven):
        // - streamSlot is an index into a global table in BIN (cpuData)
        // - stream table is u32be array at 0x144 terminated by 0xFFFFFFFF
        // - table[slot] points to a kind-first stream header (vbOff/stride/count)
        // - vbBasePtrRaw(tag=0x04) gives BIN base; vbOff is relative to that
        if (!primitiveHitsRaw.empty())
        {
            auto readU32BEAt = [&](std::size_t off) -> std::uint32_t { return readU32BE(off); };
            auto readF32BEAt = [&](std::span<const std::uint8_t> data, std::size_t off) -> float {
                if (off + 4 > data.size())
                    return 0.0f;
                std::uint32_t u = (static_cast<std::uint32_t>(data[off]) << 24) |
                                  (static_cast<std::uint32_t>(data[off + 1]) << 16) |
                                  (static_cast<std::uint32_t>(data[off + 2]) << 8) |
                                  static_cast<std::uint32_t>(data[off + 3]);
                u = swapU32(u);
                return readFloatLE(u);
            };

            auto parseStreamTable = [&](std::uint32_t tableOff, std::vector<std::uint32_t>& out) -> bool {
                out.clear();
                if (tableOff == 0 || (tableOff % 4) != 0 || tableOff + 8 > cpuData.size())
                    return false;
                constexpr std::size_t kMax = 256;
                for (std::size_t i = 0; i < kMax; ++i)
                {
                    std::size_t at = static_cast<std::size_t>(tableOff) + i * 4;
                    if (at + 4 > cpuData.size())
                        return false;
                    std::uint32_t v = readU32BEAt(at);
                    if (v == 0xFFFFFFFFu)
                        return !out.empty();
                    out.push_back(v);
                }
                return false;
            };

            std::vector<std::uint32_t> streamTable;
            bool hasTable = parseStreamTable(0x144, streamTable);
            log(std::string("ps3SlotTable @0x00000144 ") + (hasTable ? "ok" : "missing") +
                " count=" + std::to_string(streamTable.size()));

            if (hasTable)
            {
                auto readMatrixBE = [&](std::uint32_t off) -> SlLib::Math::Matrix4x4 {
                    SlLib::Math::Matrix4x4 m{};
                    if (static_cast<std::size_t>(off) + 0x40 > cpuData.size())
                        return m;
                    for (int r = 0; r < 4; ++r)
                    {
                        for (int c = 0; c < 4; ++c)
                        {
                            std::uint32_t u = readU32BEAt(static_cast<std::size_t>(off) + static_cast<std::size_t>((r * 4 + c) * 4));
                            u = swapU32(u);
                            float f = readFloatLE(u);
                            m(r, c) = f;
                        }
                    }
                    return m;
                };

                auto resolveIndexPtr = [&](std::uint32_t raw, std::span<const std::uint8_t>& outData, std::uint32_t& outOff) -> bool {
                    std::uint32_t tag = (raw >> 24) & 0xFFu;
                    std::uint32_t off24 = (raw & 0x00FFFFFFu);
                    outOff = off24;
                    if (off24 == 0)
                        return false;
                    // Observed in BH_Hard PS3:
                    //  - tag 0x1A points into GPU blob (sig)
                    //  - tag 0x08 points into CPU blob (bin)
                    if (tag == 0x1A)
                    {
                        outData = gpuData;
                        return static_cast<std::size_t>(off24) < gpuData.size();
                    }
                    if (tag == 0x08 || tag == 0x04)
                    {
                        outData = cpuData;
                        return static_cast<std::size_t>(off24) < cpuData.size();
                    }
                    return false;
                };

                std::unordered_map<std::uint64_t, std::uint64_t> seenPrimitiveKey;
                seenPrimitiveKey.reserve(2048);

                std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
                meshes.reserve(2048);

                std::size_t primitiveMatches = 0;
                std::size_t primitiveBadSlot = 0;
                std::size_t primitiveBadHeader = 0;
                std::size_t primitiveBadVb = 0;
                std::size_t primitiveBadIndex = 0;
                std::size_t primitiveMatchesU16 = 0;
                std::size_t primitiveMatchesU32 = 0;
                std::size_t primitiveSlot8 = 0;
                std::size_t primitiveSlot18 = 0;
                std::size_t primitiveSlot255 = 0;
                std::size_t primitiveSigA = 0;
                std::size_t primitiveSigB = 0;

                for (auto const& ph : primitiveHitsRaw)
                {
                    if (ph.Sig == 0x04FF3002u)
                        primitiveSigA++;
                    else if (ph.Sig == 0x04030403u)
                        primitiveSigB++;
                    if (ph.StreamSlot == 8)
                        primitiveSlot8++;
                    else if (ph.StreamSlot == 18)
                        primitiveSlot18++;
                    else if (ph.StreamSlot == 255)
                        primitiveSlot255++;

                    if (ph.StreamSlot >= streamTable.size())
                    {
                        primitiveBadSlot++;
                        continue;
                    }
                    std::uint32_t hdrOff = streamTable[ph.StreamSlot];
                    if (hdrOff + 0x28 > cpuData.size())
                    {
                        primitiveBadHeader++;
                        continue;
                    }

                    std::uint32_t kind = readU32BEAt(hdrOff + 0x00);
                    std::uint32_t vbOff = readU32BEAt(hdrOff + 0x04);
                    std::uint32_t stride = readU32BEAt(hdrOff + 0x1C);
                    std::uint32_t vcount = readU32BEAt(hdrOff + 0x20);
                    (void)kind;
                    if (stride < 12 || stride > 0x400 || vcount == 0 || vcount > 5'000'000)
                    {
                        primitiveBadHeader++;
                        continue;
                    }

                    std::uint32_t vbBaseOff = (ph.VbBasePtrRaw & 0x00FFFFFFu);
                    // In this PS3 track format, vbBasePtrRaw is expected to be a tagged pointer;
                    // we don't hard-require a tag value here, we just need the resolved base+vbOff range to be valid.
                    std::uint64_t vbStart = static_cast<std::uint64_t>(vbBaseOff) + static_cast<std::uint64_t>(vbOff);
                    std::uint64_t vbBytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(vcount);
                    if (vbStart + vbBytes > cpuData.size())
                    {
                        primitiveBadVb++;
                        continue;
                    }

                    std::span<const std::uint8_t> indexData;
                    std::uint32_t indexOff = 0;
                    if (!resolveIndexPtr(ph.IndexPtrRaw, indexData, indexOff))
                    {
                        primitiveBadIndex++;
                        continue;
                    }

                    auto canReadIndices = [&](std::size_t bytesPerIndex) -> bool {
                        std::uint64_t bytes = static_cast<std::uint64_t>(ph.NumIndices) * static_cast<std::uint64_t>(bytesPerIndex);
                        return static_cast<std::uint64_t>(indexOff) + bytes <= indexData.size();
                    };
                    auto readU16BEFrom = [&](std::size_t off) -> std::uint16_t {
                        if (off + 2 > indexData.size())
                            return 0;
                        return static_cast<std::uint16_t>((indexData[off] << 8) | indexData[off + 1]);
                    };
                    auto readU32BEFrom = [&](std::size_t off) -> std::uint32_t {
                        if (off + 4 > indexData.size())
                            return 0;
                        return (static_cast<std::uint32_t>(indexData[off]) << 24) |
                               (static_cast<std::uint32_t>(indexData[off + 1]) << 16) |
                               (static_cast<std::uint32_t>(indexData[off + 2]) << 8) |
                               static_cast<std::uint32_t>(indexData[off + 3]);
                    };
                    auto scoreIndexFormat = [&](std::size_t bytesPerIndex) -> int {
                        if (!canReadIndices(bytesPerIndex))
                            return -1;
                        std::size_t sample = std::min<std::size_t>(static_cast<std::size_t>(ph.NumIndices), 512);
                        int ok = 0;
                        for (std::size_t i = 0; i < sample; ++i)
                        {
                            std::uint32_t idx = 0;
                            if (bytesPerIndex == 2)
                                idx = readU16BEFrom(static_cast<std::size_t>(indexOff) + i * 2);
                            else
                                idx = readU32BEFrom(static_cast<std::size_t>(indexOff) + i * 4);
                            if (idx == 0xFFFFu || idx == 0xFFFFFFFFu)
                                continue;
                            if (idx < vcount)
                                ok++;
                        }
                        return ok;
                    };

                    int score16 = scoreIndexFormat(2);
                    int score32 = scoreIndexFormat(4);
                    std::size_t bytesPerIndex = (score32 > score16) ? 4 : 2;
                    if (!canReadIndices(bytesPerIndex))
                    {
                        primitiveBadIndex++;
                        continue;
                    }

                    // Unique primitive: primitiveStart + slot (in practice also avoids duplicates)
                    std::uint64_t key = (static_cast<std::uint64_t>(ph.PrimitiveStart) << 32) | static_cast<std::uint64_t>(ph.StreamSlot);
                    if (seenPrimitiveKey.find(key) != seenPrimitiveKey.end())
                        continue;
                    seenPrimitiveKey.emplace(key, 1);

                    Renderer::SlRenderer::ForestCpuMesh mesh;
                    mesh.Skinned = false;
                    mesh.Model = readMatrixBE(ph.PrimitiveStart);
                    mesh.Vertices.resize(static_cast<std::size_t>(vcount) * 20, 0.0f);

                    std::span<const std::uint8_t> vbData = cpuData;
                    for (std::uint32_t vi = 0; vi < vcount; ++vi)
                    {
                        std::size_t base = static_cast<std::size_t>(vbStart) + static_cast<std::size_t>(vi) * static_cast<std::size_t>(stride);
                        if (base + 12 > vbData.size())
                            break;
                        float x = readF32BEAt(vbData, base + 0);
                        float y = readF32BEAt(vbData, base + 4);
                        float z = readF32BEAt(vbData, base + 8);
                        std::size_t out = static_cast<std::size_t>(vi) * 20;
                        mesh.Vertices[out + 0] = x;
                        mesh.Vertices[out + 1] = y;
                        mesh.Vertices[out + 2] = z;
                        mesh.Vertices[out + 3] = 0.0f;
                        mesh.Vertices[out + 4] = 1.0f;
                        mesh.Vertices[out + 5] = 0.0f;
                        mesh.Vertices[out + 6] = 0.0f;
                        mesh.Vertices[out + 7] = 0.0f;
                        mesh.Vertices[out + 8] = 1.0f;
                        mesh.Vertices[out + 9] = 0.0f;
                        mesh.Vertices[out + 10] = 0.0f;
                        mesh.Vertices[out + 11] = 0.0f;
                    }

                    mesh.Indices.reserve(ph.NumIndices);
                    for (std::uint32_t ii = 0; ii < ph.NumIndices; ++ii)
                    {
                        std::uint32_t idx = 0;
                        if (bytesPerIndex == 2)
                        {
                            idx = readU16BEFrom(static_cast<std::size_t>(indexOff) + static_cast<std::size_t>(ii) * 2);
                            if (idx == 0xFFFFu)
                                continue;
                        }
                        else
                        {
                            idx = readU32BEFrom(static_cast<std::size_t>(indexOff) + static_cast<std::size_t>(ii) * 4);
                            if (idx == 0xFFFFFFFFu)
                                continue;
                        }
                        if (idx >= vcount)
                            continue;
                        mesh.Indices.push_back(idx);
                    }

                    if (mesh.Indices.size() >= 3)
                    {
                        meshes.push_back(std::move(mesh));
                        primitiveMatches++;
                        if (bytesPerIndex == 2)
                            primitiveMatchesU16++;
                        else
                            primitiveMatchesU32++;
                    }

                    if (meshes.size() >= 2048)
                        break;
                }

                log("PS3 slot-table matches=" + std::to_string(primitiveMatches) +
                    " meshes=" + std::to_string(meshes.size()) +
                    " (u16=" + std::to_string(primitiveMatchesU16) +
                    " u32=" + std::to_string(primitiveMatchesU32) +
                    " sigA=" + std::to_string(primitiveSigA) +
                    " sigB=" + std::to_string(primitiveSigB) +
                    " slot8=" + std::to_string(primitiveSlot8) +
                    " slot18=" + std::to_string(primitiveSlot18) +
                    " slot255=" + std::to_string(primitiveSlot255) +
                    " badSlot=" + std::to_string(primitiveBadSlot) +
                    " badHdr=" + std::to_string(primitiveBadHeader) +
                    " badVb=" + std::to_string(primitiveBadVb) +
                    " badIdx=" + std::to_string(primitiveBadIndex) + ")");
                std::cout << "[ForestPS3] slotTable meshesBuilt=" << meshes.size()
                          << " (see ForestPS3.log)\n";

                if (!meshes.empty())
                {
                    _forestLibrary.reset();
                    _forestMeshSources.clear();
                    _allForestMeshes = meshes;
                    _renderer.SetForestMeshes(std::move(meshes));
                    _renderer.SetDrawForestMeshes(true);
                    _drawForestMeshes = true;

                    _forestHierarchy.clear();
                    ForestHierarchy fh;
                    fh.Name = "PS3 Forest (slot-table)";
                    fh.Visible = true;
                    _forestHierarchy.push_back(std::move(fh));
                    return;
                }
            }
        }

        // If the deterministic slot-table path didn't render anything, stop here for now.
        // (The older pointer-based stream discovery used fields that are not valid for this PS3 track format.)
        log("PS3 slot-table path did not produce meshes; stopping (no fallback renderer yet).");
        return;

#if 0
        auto determineSwapForStream = [&](Ps3StreamHeader const& s) -> bool {
            int plausibleSwapped = 0;
            int plausibleRaw = 0;
            int sample = static_cast<int>(std::min<std::uint32_t>(s.count, 32));
            std::span<const std::uint8_t> vb = s.vertexInGpu ? gpuData : cpuData;
            auto readU32BEFrom = [&](std::size_t off) -> std::uint32_t {
                if (off + 4 > vb.size())
                    return 0;
                return (static_cast<std::uint32_t>(vb[off]) << 24) |
                       (static_cast<std::uint32_t>(vb[off + 1]) << 16) |
                       (static_cast<std::uint32_t>(vb[off + 2]) << 8) |
                       static_cast<std::uint32_t>(vb[off + 3]);
            };
            for (int vi = 0; vi < sample; ++vi)
            {
                std::size_t base = static_cast<std::size_t>(s.vertexDataPtr) + static_cast<std::size_t>(vi) *
                                   static_cast<std::size_t>(s.stride);
                if (base + 12 > vb.size())
                    break;
                std::uint32_t ux = readU32BEFrom(base + 0);
                std::uint32_t uy = readU32BEFrom(base + 4);
                std::uint32_t uz = readU32BEFrom(base + 8);
                float xRaw = readFloatLE(ux);
                float yRaw = readFloatLE(uy);
                float zRaw = readFloatLE(uz);
                float xSw = readFloatLE(swapU32(ux));
                float ySw = readFloatLE(swapU32(uy));
                float zSw = readFloatLE(swapU32(uz));
                auto ok = [](float v) { return std::isfinite(v) && std::abs(v) < 1.0e6f; };
                if (ok(xRaw) && ok(yRaw) && ok(zRaw))
                    plausibleRaw++;
                if (ok(xSw) && ok(ySw) && ok(zSw))
                    plausibleSwapped++;
            }
            return plausibleSwapped > plausibleRaw;
        };

        constexpr std::size_t kPrimitiveVertexStreamField = 0x9C;
        constexpr std::size_t kPrimitiveNumIndicesField = 0x90;
        constexpr std::size_t kPrimitiveIndexPtrField = 0x94;

        std::unordered_map<std::uint64_t, std::uint64_t> seenPrimitiveKey;
        seenPrimitiveKey.reserve(1024);

        std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
        meshes.reserve(2048);
        std::size_t primitiveMatches = 0;
        std::size_t primitiveMatchesU16 = 0;
        std::size_t primitiveMatchesU32 = 0;
        std::size_t primitiveIndexUnknown = 0;

        auto readU16BEAt = [&](std::span<const std::uint8_t> data, std::size_t off) -> std::uint16_t {
            if (off + 2 > data.size())
                return 0;
            return static_cast<std::uint16_t>((data[off] << 8) | data[off + 1]);
        };

        auto readMatrixBE = [&](std::uint32_t off) -> SlLib::Math::Matrix4x4 {
            SlLib::Math::Matrix4x4 m{};
            if (off + 0x40 > cpuData.size())
                return m;
            for (int r = 0; r < 4; ++r)
            {
                for (int c = 0; c < 4; ++c)
                {
                    std::uint32_t u = readU32BE(static_cast<std::size_t>(off) + static_cast<std::size_t>((r * 4 + c) * 4));
                    u = swapU32(u);
                    float f = readFloatLE(u);
                    m(r, c) = f;
                }
            }
            return m;
        };

        // Build meshes from the primitive candidates found during the PS3 graph walk.
        // Do NOT rely on the SIF relocation list here (it is incomplete for PS3 tracks).
        for (auto const& ph : primitiveHitsRaw)
        {
            Ps3Ptr vsPtr = decodePs3Ptr(ph.VertexStreamPtrRaw);
            Ps3Ptr idxPtr = decodePs3Ptr(ph.IndexPtrRaw);
            if (!vsPtr.Valid || vsPtr.IsGpu)
                continue;
            if (!idxPtr.Valid || idxPtr.Offset == 0)
                continue;

            auto itStream = streamIndex.find(vsPtr.Offset);
            if (itStream == streamIndex.end())
                continue;

            std::uint32_t primitiveStart = ph.PrimitiveStart;
            if (primitiveStart == 0 || static_cast<std::size_t>(primitiveStart) + 0xA8 > cpuData.size())
                continue;

            std::uint32_t numIndices = ph.NumIndices;
            if (numIndices < 3 || numIndices > 2'000'000)
                continue;

            std::span<const std::uint8_t> indexData = idxPtr.IsGpu ? gpuData : cpuData;

            auto canReadIndices = [&](std::size_t bytesPerIndex) -> bool {
                std::uint64_t bytes = static_cast<std::uint64_t>(numIndices) * static_cast<std::uint64_t>(bytesPerIndex);
                return static_cast<std::uint64_t>(idxPtr.Offset) + bytes <= indexData.size();
            };

            Ps3StreamHeader const& stream = streams[itStream->second];
            bool swapFloats = determineSwapForStream(stream);

            auto scoreIndexFormat = [&](std::size_t bytesPerIndex) -> int {
                if (!canReadIndices(bytesPerIndex))
                    return -1;
                std::size_t sample = std::min<std::size_t>(static_cast<std::size_t>(numIndices), 512);
                int ok = 0;
                for (std::size_t i = 0; i < sample; ++i)
                {
                    std::uint32_t idx = 0;
                    if (bytesPerIndex == 2)
                    {
                        idx = readU16BEAt(indexData, static_cast<std::size_t>(idxPtr.Offset) + i * 2);
                    }
                    else
                    {
                        std::size_t off = static_cast<std::size_t>(idxPtr.Offset) + i * 4;
                        if (off + 4 > indexData.size())
                            break;
                        idx = (static_cast<std::uint32_t>(indexData[off]) << 24) |
                              (static_cast<std::uint32_t>(indexData[off + 1]) << 16) |
                              (static_cast<std::uint32_t>(indexData[off + 2]) << 8) |
                              static_cast<std::uint32_t>(indexData[off + 3]);
                    }
                    // treat some common sentinels as "ok" (restart / padding)
                    if (idx == 0xFFFFu || idx == 0xFFFFFFFFu)
                        continue;
                    if (idx < stream.count)
                        ok++;
                }
                return ok;
            };

            // Uniqueness: primitiveStart + vsPtr should be unique.
            std::uint64_t key = (static_cast<std::uint64_t>(primitiveStart) << 32) | static_cast<std::uint64_t>(vsPtr.Offset);
            if (seenPrimitiveKey.find(key) != seenPrimitiveKey.end())
                continue;
            seenPrimitiveKey.emplace(key, 1);

            int score16 = scoreIndexFormat(2);
            int score32 = scoreIndexFormat(4);
            std::size_t bytesPerIndex = (score32 > score16) ? 4 : 2;
            if (!canReadIndices(bytesPerIndex))
            {
                primitiveIndexUnknown++;
                continue;
            }

            // Build vertex buffer (pos only, other channels default).
            Renderer::SlRenderer::ForestCpuMesh mesh;
            mesh.Skinned = false;
            mesh.Model = readMatrixBE(primitiveStart);
            std::span<const std::uint8_t> vbData = stream.vertexInGpu ? gpuData : cpuData;
            mesh.Vertices.resize(static_cast<std::size_t>(stream.count) * 20, 0.0f);
            for (std::uint32_t vi = 0; vi < stream.count; ++vi)
            {
                std::size_t base = static_cast<std::size_t>(stream.vertexDataPtr) + static_cast<std::size_t>(vi) *
                                   static_cast<std::size_t>(stream.stride);
                if (base + 12 > vbData.size())
                    break;

                auto readU32BEFrom = [&](std::size_t off) -> std::uint32_t {
                    if (off + 4 > vbData.size())
                        return 0;
                    return (static_cast<std::uint32_t>(vbData[off]) << 24) |
                           (static_cast<std::uint32_t>(vbData[off + 1]) << 16) |
                           (static_cast<std::uint32_t>(vbData[off + 2]) << 8) |
                           static_cast<std::uint32_t>(vbData[off + 3]);
                };
                std::uint32_t ux = readU32BEFrom(base + 0);
                std::uint32_t uy = readU32BEFrom(base + 4);
                std::uint32_t uz = readU32BEFrom(base + 8);
                if (swapFloats)
                {
                    ux = swapU32(ux);
                    uy = swapU32(uy);
                    uz = swapU32(uz);
                }
                float x = readFloatLE(ux);
                float y = readFloatLE(uy);
                float z = readFloatLE(uz);

                std::size_t out = static_cast<std::size_t>(vi) * 20;
                mesh.Vertices[out + 0] = x;
                mesh.Vertices[out + 1] = y;
                mesh.Vertices[out + 2] = z;
                // normal
                mesh.Vertices[out + 3] = 0.0f;
                mesh.Vertices[out + 4] = 1.0f;
                mesh.Vertices[out + 5] = 0.0f;
                // uv
                mesh.Vertices[out + 6] = 0.0f;
                mesh.Vertices[out + 7] = 0.0f;
                // weights
                mesh.Vertices[out + 8] = 1.0f;
                mesh.Vertices[out + 9] = 0.0f;
                mesh.Vertices[out + 10] = 0.0f;
                mesh.Vertices[out + 11] = 0.0f;
                // indices (as floats)
                mesh.Vertices[out + 12] = 0.0f;
                mesh.Vertices[out + 13] = 0.0f;
                mesh.Vertices[out + 14] = 0.0f;
                mesh.Vertices[out + 15] = 0.0f;
                // pad remaining (16..19) if renderer expects 20 floats; keep 0.
            }

            mesh.Indices.reserve(numIndices);
            for (std::uint32_t ii = 0; ii < numIndices; ++ii)
            {
                std::uint32_t idx = 0;
                if (bytesPerIndex == 2)
                {
                    idx = readU16BEAt(indexData, static_cast<std::size_t>(idxPtr.Offset) + static_cast<std::size_t>(ii) * 2);
                    if (idx == 0xFFFFu)
                        continue;
                }
                else
                {
                    std::size_t off = static_cast<std::size_t>(idxPtr.Offset) + static_cast<std::size_t>(ii) * 4;
                    if (off + 4 > indexData.size())
                        break;
                    idx = (static_cast<std::uint32_t>(indexData[off]) << 24) |
                          (static_cast<std::uint32_t>(indexData[off + 1]) << 16) |
                          (static_cast<std::uint32_t>(indexData[off + 2]) << 8) |
                          static_cast<std::uint32_t>(indexData[off + 3]);
                    if (idx == 0xFFFFFFFFu)
                        continue;
                }

                if (idx >= stream.count)
                    continue;
                mesh.Indices.push_back(idx);
            }
            if (mesh.Indices.size() >= 3)
            {
                meshes.push_back(std::move(mesh));
                primitiveMatches++;
                if (bytesPerIndex == 2)
                    primitiveMatchesU16++;
                else
                    primitiveMatchesU32++;
            }

            if (meshes.size() >= 2048)
                break;
        }

        log("Primitive matches: " + std::to_string(primitiveMatches) +
            " (u16=" + std::to_string(primitiveMatchesU16) +
            " u32=" + std::to_string(primitiveMatchesU32) +
            " unk=" + std::to_string(primitiveIndexUnknown) + ")" +
            " meshes=" + std::to_string(meshes.size()));
        std::cout << "[ForestPS3] primitiveMatches=" << primitiveMatches
                  << " meshesBuilt=" << meshes.size() << " (see ForestPS3.log)\n";
        if (meshes.empty())
        {
            log("No meshes built (likely index/stream pairing mismatch).");
            std::cout << "[ForestPS3] No meshes built (see ForestPS3.log)\n";
            return;
        }

        _forestLibrary.reset();
        _forestMeshSources.clear();
        _allForestMeshes = meshes;
        _renderer.SetForestMeshes(std::move(meshes));
        _renderer.SetDrawForestMeshes(true);
        _drawForestMeshes = true;

        // Populate minimal hierarchy so the Hierarchy tab isn't empty on PS3.
        _forestHierarchy.clear();
        ForestHierarchy fh;
        fh.Name = "PS3 Forest (primitive-matched)";
        fh.Visible = true;
        _forestHierarchy.push_back(std::move(fh));

        // Optional: also show boxes as a debug aid (off by default).
        _drawForestBoxes = false;
        _forestBoxLayers.clear();
        _renderer.SetForestBoxes({});
        _renderer.SetDrawForestBoxes(false);
        return;
#endif
    }

    auto library = std::make_shared<SeEditor::Forest::ForestLibrary>();
    try
    {
        library->Load(context);
    }
    catch (std::exception const& ex)
    {
        std::cerr << "[CharmyBee] Forest load failed: " << ex.what() << std::endl;
        return;
    }

    struct ForestVertex
    {
        SlLib::Math::Vector3 Pos{};
        SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
        SlLib::Math::Vector2 Uv{};
        std::array<float, 4> Weights{1.0f, 0.0f, 0.0f, 0.0f};
        std::array<float, 4> Indices{0.0f, 0.0f, 0.0f, 0.0f};
    };

    struct BoxState
    {
        bool Has = false;
        SlLib::Math::Vector3 Min{};
        SlLib::Math::Vector3 Max{};

        void Include(SlLib::Math::Vector3 const& point)
        {
            if (!Has)
            {
                Min = Max = point;
                Has = true;
                return;
            }
            Min.X = std::min(Min.X, point.X);
            Min.Y = std::min(Min.Y, point.Y);
            Min.Z = std::min(Min.Z, point.Z);
            Max.X = std::max(Max.X, point.X);
            Max.Y = std::max(Max.Y, point.Y);
            Max.Z = std::max(Max.Z, point.Z);
        }
    };

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
    auto readU8 = [](std::vector<std::uint8_t> const& data, std::size_t offset) -> std::uint8_t {
        if (offset >= data.size())
            return 0;
        return data[offset];
    };

    auto decodeVertex = [&](SeEditor::Forest::SuRenderVertexStream const& stream) {
        std::vector<ForestVertex> verts;
        if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
            return verts;

        verts.resize(static_cast<std::size_t>(stream.VertexCount));
        for (int i = 0; i < stream.VertexCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
            ForestVertex v;
            for (auto const& attr : stream.AttributeStreamsInfo)
            {
                if (attr.Stream != 0)
                    continue;

                std::size_t off = base + static_cast<std::size_t>(attr.Offset);
                using SeEditor::Forest::D3DDeclType;
                using SeEditor::Forest::D3DDeclUsage;

                if (attr.Usage == D3DDeclUsage::Position)
                {
                    std::size_t posOff = off;
                    if (stream.StreamBias != 0)
                        posOff += static_cast<std::size_t>(stream.StreamBias);
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::Normal)
                {
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Normal = {readFloat(stream.Stream, off + 0),
                                    readFloat(stream.Stream, off + 4),
                                    readFloat(stream.Stream, off + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x4)
                    {
                        v.Normal = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                    HalfToFloat(readU16(stream.Stream, off + 2)),
                                    HalfToFloat(readU16(stream.Stream, off + 4))};
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
                    else if (attr.Type == D3DDeclType::Float16x2)
                    {
                        v.Uv = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                HalfToFloat(readU16(stream.Stream, off + 2))};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::BlendWeight)
                {
                    if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Weights = {readFloat(stream.Stream, off + 0),
                                     readFloat(stream.Stream, off + 4),
                                     readFloat(stream.Stream, off + 8),
                                     readFloat(stream.Stream, off + 12)};
                    }
                    else if (attr.Type == D3DDeclType::UByte4N)
                    {
                        v.Weights = {readU8(stream.Stream, off + 0) / 255.0f,
                                     readU8(stream.Stream, off + 1) / 255.0f,
                                     readU8(stream.Stream, off + 2) / 255.0f,
                                     readU8(stream.Stream, off + 3) / 255.0f};
                    }
                    else if (attr.Type == D3DDeclType::Short4N)
                    {
                        v.Weights = {readS16(stream.Stream, off + 0) / 32767.0f,
                                     readS16(stream.Stream, off + 2) / 32767.0f,
                                     readS16(stream.Stream, off + 4) / 32767.0f,
                                     readS16(stream.Stream, off + 6) / 32767.0f};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::BlendIndices)
                {
                    if (attr.Type == D3DDeclType::UByte4 || attr.Type == D3DDeclType::UByte4N)
                    {
                        v.Indices = {static_cast<float>(readU8(stream.Stream, off + 0)),
                                     static_cast<float>(readU8(stream.Stream, off + 1)),
                                     static_cast<float>(readU8(stream.Stream, off + 2)),
                                     static_cast<float>(readU8(stream.Stream, off + 3))};
                    }
                    else if (attr.Type == D3DDeclType::Short4)
                    {
                        v.Indices = {static_cast<float>(readS16(stream.Stream, off + 0)),
                                     static_cast<float>(readS16(stream.Stream, off + 2)),
                                     static_cast<float>(readS16(stream.Stream, off + 4)),
                                     static_cast<float>(readS16(stream.Stream, off + 6))};
                    }
                }
            }

            float sum = v.Weights[0] + v.Weights[1] + v.Weights[2] + v.Weights[3];
            if (sum > 0.0f)
            {
                float inv = 1.0f / sum;
                for (auto& w : v.Weights)
                    w *= inv;
            }
            verts[static_cast<std::size_t>(i)] = v;
        }

        return verts;
    };

    auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
        auto clamp = [](float v) { return (std::abs(v) < 1e-4f) ? 1.0f : v; };
        auto safe = [](float v, float fallback) { return std::isfinite(v) ? v : fallback; };
        t.X = safe(t.X, 0.0f);
        t.Y = safe(t.Y, 0.0f);
        t.Z = safe(t.Z, 0.0f);
        s.X = clamp(s.X);
        s.Y = clamp(s.Y);
        s.Z = clamp(s.Z);
        s.X = safe(s.X, 1.0f);
        s.Y = safe(s.Y, 1.0f);
        s.Z = safe(s.Z, 1.0f);
        SlLib::Math::Quaternion q{r.X, r.Y, r.Z, r.W};
        float qLen = std::sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
        if (!std::isfinite(qLen) || qLen < 1e-6f)
        {
            q = {0.0f, 0.0f, 0.0f, 1.0f};
        }
        else
        {
            float invLen = 1.0f / qLen;
            q = q * invLen;
        }
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

    std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
    std::vector<ForestMeshSource> meshSources;
    std::vector<ForestBoxLayer> layers;
    layers.reserve(library->Forests.size());
    std::size_t debugDroppedLogged = 0;

    for (std::size_t forestIdx = 0; forestIdx < library->Forests.size(); ++forestIdx)
    {
        auto const& forestEntry = library->Forests[forestIdx];
        if (!forestEntry.Forest)
            continue;

        ForestBoxLayer layer;
        layer.Name = forestEntry.Name.empty()
                         ? std::string("Forest ") + std::to_string(forestIdx)
                         : forestEntry.Name;
        layer.ForestIndex = static_cast<int>(forestIdx);
        BoxState forestState;
        auto const& trees = forestEntry.Forest->Trees;
        struct TreeInfo
        {
            BoxState Bounds;
            std::size_t MeshStart = 0;
            std::size_t MeshCount = 0;
        };
        std::vector<TreeInfo> treeInfos(trees.size());

        for (std::size_t treeIdx = 0; treeIdx < trees.size(); ++treeIdx)
        {
            auto const& tree = trees[treeIdx];
            if (!tree)
                continue;

            TreeInfo& treeInfo = treeInfos[treeIdx];
            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);
            std::size_t treeMeshStart = meshes.size();

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

                auto local = buildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        SlLib::Math::Multiply(computeWorld(parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                  SlLib::Math::Matrix4x4 const& worldMatrix,
                                  int branchIndex,
                                  int lodIndex) {
                if (!mesh)
                    return;

                for (std::size_t primIndex = 0; primIndex < mesh->Primitives.size(); ++primIndex)
                {
                    auto const& primitive = mesh->Primitives[primIndex];
                    if (!primitive || !primitive->VertexStream)
                        continue;

                    auto verts = decodeVertex(*primitive->VertexStream);
                    if (verts.empty())
                        continue;

                    for (auto const& v : verts)
                    {
                        SlLib::Math::Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                        auto transformed = SlLib::Math::Transform(worldMatrix, pos4);
                        SlLib::Math::Vector3 p{transformed.X, transformed.Y, transformed.Z};
                        treeInfo.Bounds.Include(p);
                        forestState.Include(p);
                    }

                    bool skinned = !mesh->BoneMatrixIndices.empty() && !mesh->BoneInverseMatrices.empty();

                    ForestMeshSource source;
                    source.Skinned = skinned;
                    if (skinned)
                    {
                        source.BoneMatrixIndices = mesh->BoneMatrixIndices;
                        source.BoneInverseMatrices = mesh->BoneInverseMatrices;
                    }
                    source.Vertices.reserve(verts.size() * 16);
                    for (auto const& v : verts)
                    {
                        source.Vertices.push_back(v.Pos.X);
                        source.Vertices.push_back(v.Pos.Y);
                        source.Vertices.push_back(v.Pos.Z);
                        source.Vertices.push_back(v.Normal.X);
                        source.Vertices.push_back(v.Normal.Y);
                        source.Vertices.push_back(v.Normal.Z);
                        source.Vertices.push_back(v.Uv.X);
                        source.Vertices.push_back(v.Uv.Y);
                        source.Vertices.insert(source.Vertices.end(), v.Weights.begin(), v.Weights.end());
                        source.Vertices.insert(source.Vertices.end(), v.Indices.begin(), v.Indices.end());
                    }

                    Renderer::SlRenderer::ForestCpuMesh cpu;
                    cpu.Model = skinned ? IdentityMatrix() : worldMatrix;
                    cpu.Skinned = skinned;
                    if (skinned)
                    {
                        cpu.BoneMatrixIndices = mesh->BoneMatrixIndices;
                        cpu.BoneInverseMatrices = mesh->BoneInverseMatrices;
                    }
                    cpu.Vertices.reserve(verts.size() * 16);
                    for (auto const& v : verts)
                    {
                        cpu.Vertices.push_back(v.Pos.X);
                        cpu.Vertices.push_back(v.Pos.Y);
                        cpu.Vertices.push_back(v.Pos.Z);
                        cpu.Vertices.push_back(v.Normal.X);
                        cpu.Vertices.push_back(v.Normal.Y);
                        cpu.Vertices.push_back(v.Normal.Z);
                        cpu.Vertices.push_back(v.Uv.X);
                        cpu.Vertices.push_back(v.Uv.Y);
                        cpu.Vertices.insert(cpu.Vertices.end(), v.Weights.begin(), v.Weights.end());
                        cpu.Vertices.insert(cpu.Vertices.end(), v.Indices.begin(), v.Indices.end());
                    }

                    std::size_t availableIndices = primitive->IndexData.size() / 2;
                    std::size_t indexCount = availableIndices;
                    if (primitive->NumIndices > 0)
                    {
                        if (static_cast<std::size_t>(primitive->NumIndices) > availableIndices)
                        {
                            std::cerr << "[Forest] Primitive index count " << primitive->NumIndices
                                      << " exceeds buffer (" << availableIndices << "), clamping.\n";
                        }
                        indexCount = std::min(static_cast<std::size_t>(primitive->NumIndices), availableIndices);
                    }

                    if (primitive->NumIndices <= 0 && availableIndices > 0)
                    {
                        std::cerr << "[Forest] Primitive reports zero indices but buffer contains "
                                  << availableIndices << " entries.\n";
                    }

                    if (indexCount == 0)
                        continue;

                    std::size_t vertexLimit = verts.size();
                    struct IndexMode
                    {
                        bool Use32 = false;
                        bool Swap = false;
                        std::size_t Count = 0;
                        std::size_t Droppable = 0;
                        std::size_t Restart = 0;
                        std::uint32_t MaxIndex = 0;
                    };

                    auto eval16 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = false;
                        mode.Swap = swap;
                        mode.Count = indexCount;
                        for (std::size_t i = 0; i < indexCount; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swap ? static_cast<std::uint16_t>((a << 8) | b)
                                                     : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++mode.Restart;
                                continue;
                            }
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                                ++mode.Droppable;
                        }
                        return mode;
                    };

                    auto eval32 = [&](bool swap) {
                        IndexMode mode;
                        mode.Use32 = true;
                        mode.Swap = swap;
                        if (primitive->IndexData.size() % 4 != 0)
                            return mode;
                        mode.Count = primitive->IndexData.size() / 4;
                        if (primitive->NumIndices > 0)
                            mode.Count = std::min<std::size_t>(mode.Count,
                                static_cast<std::size_t>(primitive->NumIndices));
                        for (std::size_t i = 0; i < mode.Count; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
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
                            if (idx > mode.MaxIndex)
                                mode.MaxIndex = idx;
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
                    std::size_t droppable = 0;
                    std::size_t restart = 0;
                    std::vector<std::uint32_t> rawIndices;
                    rawIndices.reserve(best.Count);

                    if (use32Bit)
                    {
                        for (std::size_t i = 0; i < indexCount32; ++i)
                        {
                            std::size_t off = i * 4;
                            std::uint32_t idx = static_cast<std::uint32_t>(primitive->IndexData[off + 0] |
                                (primitive->IndexData[off + 1] << 8) |
                                (primitive->IndexData[off + 2] << 16) |
                                (primitive->IndexData[off + 3] << 24));
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
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(idx);
                        }
                    }
                    else
                    {
                        for (std::size_t i = 0; i < best.Count; ++i)
                        {
                            std::uint16_t a = primitive->IndexData[i * 2];
                            std::uint16_t b = primitive->IndexData[i * 2 + 1];
                            std::uint16_t idx = swapIndices ? static_cast<std::uint16_t>((a << 8) | b)
                                                            : static_cast<std::uint16_t>(a | (b << 8));
                            if (idx == 0xFFFFu)
                            {
                                ++restart;
                                rawIndices.push_back(idx);
                                continue;
                            }
                            if (static_cast<std::size_t>(idx) >= vertexLimit)
                            {
                                ++droppable;
                                continue;
                            }
                            rawIndices.push_back(static_cast<std::uint32_t>(idx));
                        }
                    }

                    int primitiveType = primitive->Unknown_0x9c;
                    bool isStrip = primitiveType == 5 || (primitiveType != 4 && restart > 0);
                    if (isStrip)
                    {
                        cpu.Indices.reserve(rawIndices.size());
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
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(idx);
                                }
                                else
                                {
                                    cpu.Indices.push_back(i0);
                                    cpu.Indices.push_back(i1);
                                    cpu.Indices.push_back(idx);
                                }
                            }
                            i0 = i1;
                            i1 = idx;
                            flip = !flip;
                        }
                    }
                    else
                    {
                        cpu.Indices.reserve(rawIndices.size());
                        for (std::uint32_t idx : rawIndices)
                        {
                            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                                continue;
                            cpu.Indices.push_back(idx);
                        }
                    }

                    if (droppable > 0 || restart > 0)
                    {
                        if (droppable > 0)
                        {
                            std::cerr << "[Forest] Dropped " << droppable << " indices that referenced "
                                      << vertexLimit << " vertices. "
                                      << "forest=" << forestIdx
                                      << " name=" << forestEntry.Name
                                      << " tree=" << treeIdx
                                      << " treeHash=" << tree->Hash
                                      << " branch=" << branchIndex
                                      << " lod=" << lodIndex
                                      << " prim=" << primIndex
                                      << '\n';
                        }
                        if (restart > 0)
                        {
                            std::cerr << "[Forest] Skipped " << restart << " primitive-restart indices. "
                                      << "forest=" << forestIdx
                                      << " name=" << forestEntry.Name
                                      << " tree=" << treeIdx
                                      << " treeHash=" << tree->Hash
                                      << " branch=" << branchIndex
                                      << " lod=" << lodIndex
                                      << " prim=" << primIndex
                                      << '\n';
                        }
                        if (debugDroppedLogged < 5 && primitive->VertexStream)
                        {
                            ++debugDroppedLogged;
                            std::cerr << "[Forest] Debug: indices16 max=" << mode16le.MaxIndex
                                      << " dropped16=" << mode16le.Droppable
                                      << " restart16=" << mode16le.Restart
                                      << " indices32 max=" << mode32le.MaxIndex
                                      << " dropped32=" << mode32le.Droppable
                                      << " restart32=" << mode32le.Restart
                                      << " use32=" << (use32Bit ? "true" : "false")
                                      << " swap=" << (swapIndices ? "true" : "false")
                                      << " vtxCount=" << primitive->VertexStream->VertexCount
                                      << " stride=" << primitive->VertexStream->VertexStride
                                      << " streamBias=" << primitive->VertexStream->StreamBias
                                      << " endian=" << (isBigEndian ? "BE" : "LE")
                                      << " primType=" << primitiveType
                                      << " forest=" << forestIdx
                                      << " name=" << forestEntry.Name
                                      << " tree=" << treeIdx
                                      << " treeHash=" << tree->Hash
                                      << " branch=" << branchIndex
                                      << " lod=" << lodIndex
                                      << " prim=" << primIndex
                                      << '\n';
                        }
                    }

                    if (cpu.Indices.empty())
                        continue;

                    if (primitive->Material && !primitive->Material->Textures.empty())
                        cpu.Texture = primitive->Material->Textures[0]->TextureResource;
                    cpu.ForestIndex = static_cast<int>(forestIdx);
                    cpu.TreeIndex = static_cast<int>(treeIdx);
                    cpu.BranchIndex = branchIndex;
                    meshes.push_back(std::move(cpu));

                    if (primitive->Material && !primitive->Material->Textures.empty())
                        source.Texture = primitive->Material->Textures[0]->TextureResource;
                    source.ForestIndex = static_cast<int>(forestIdx);
                    source.TreeIndex = static_cast<int>(treeIdx);
                    source.BranchIndex = branchIndex;
                    source.Indices = meshes.back().Indices;
                    meshSources.push_back(std::move(source));
                }
            };

            for (std::size_t i = 0; i < branchCount; ++i)
            {
                auto worldMatrix = computeWorld(static_cast<int>(i));
                auto const& branch = tree->Branches[i];
                if (!branch)
                    continue;

                if (branch->Mesh)
                    appendMesh(branch->Mesh, worldMatrix, static_cast<int>(i), -1);
                if (branch->Lod)
                {
                    for (std::size_t lodIdx = 0; lodIdx < branch->Lod->Thresholds.size(); ++lodIdx)
                    {
                        auto const& threshold = branch->Lod->Thresholds[lodIdx];
                        if (threshold && threshold->Mesh)
                            appendMesh(threshold->Mesh, worldMatrix, static_cast<int>(i), static_cast<int>(lodIdx));
                    }
                }
            }

            treeInfo.MeshStart = treeMeshStart;
            treeInfo.MeshCount = meshes.size() - treeMeshStart;
        }

        for (std::size_t treeIdx = 0; treeIdx < treeInfos.size(); ++treeIdx)
        {
            auto const& treeInfo = treeInfos[treeIdx];
            if (!treeInfo.Bounds.Has)
                continue;

            ForestBoxLayer child;
            child.Name = std::string("Tree ") + std::to_string(treeIdx);
            child.Visible = true;
            child.HasBounds = true;
            child.Min = treeInfo.Bounds.Min;
            child.Max = treeInfo.Bounds.Max;
            child.MeshStartIndex = treeInfo.MeshStart;
            child.MeshCount = treeInfo.MeshCount;
            child.ForestIndex = layer.ForestIndex;
            child.TreeIndex = static_cast<int>(treeIdx);
            layer.Children.push_back(std::move(child));
        }

        if (forestState.Has)
        {
            layer.HasBounds = true;
            layer.Min = forestState.Min;
            layer.Max = forestState.Max;
        }

        if (layer.HasBounds || !layer.Children.empty())
            layers.push_back(std::move(layer));
    }

    if (!meshes.empty())
    {
        _allForestMeshes = std::move(meshes);
        _forestMeshSources = std::move(meshSources);
        _drawForestMeshes = true;
        _forestLibrary = std::move(library);
        std::cout << "[CharmyBee] Forest meshes loaded: " << _forestLibrary->Forests.size() << " forests." << std::endl;
    }
    else
    {
        _allForestMeshes.clear();
        _forestMeshSources.clear();
    }

    _forestBoxLayers = std::move(layers);
    LoadForestVisibility();
    UpdateForestHierarchy();
    LoadForestHierarchyVisibility();
    LoadAnimatorSettings();
    ApplyTreeVisibilityToLayers();
    UpdateForestBoxRenderer();
    UpdateForestMeshRendering();
    _animatorDirty = true;
}


void CharmyBee::LoadNavigationResources()
{
    if (_sifChunks.empty())
    {
        ReportSifError("No SIF chunks loaded.");
        return;
    }

    NavigationProbeInfo probe{};
    auto navigation = std::make_unique<SlLib::SumoTool::Siff::Navigation>();
    std::string error;
    if (!LoadNavigationFromSifChunks(_sifChunks, *navigation, probe, error))
    {
        ReportSifError(error);
        return;
    }

    std::cout << "[Navigation] base=0x" << std::hex << probe.BaseOffset
              << " version=" << std::dec << probe.Version
              << " score=" << probe.Score
              << " waypoints=" << probe.NumWaypoints
              << " racingLines=" << probe.NumRacingLines
              << std::endl;

    _sifNavigation = std::move(navigation);
    _sifNavigationTool = std::make_unique<Editor::Tools::NavigationTool>(_sifNavigation.get());

    _navigationLineEntries.clear();
    if (_sifNavigation)
    {
        for (std::size_t i = 0; i < _sifNavigation->RacingLines.size(); ++i)
        {
            std::string label = "Racing Line " + std::to_string(i);
            auto const& line = _sifNavigation->RacingLines[i];
            if (line)
                label += " (" + std::to_string(line->Segments.size()) + " segments)";
            _navigationLineEntries.push_back({static_cast<int>(i), std::move(label), true});
        }
    }

    UpdateNavigationLineVisibility();
    _drawNavigation = true;
    UpdateDebugLines();

    if (_sifNavigation)
    {
        bool hasPoint = false;
        SlLib::Math::Vector3 min{};
        SlLib::Math::Vector3 max{};
        auto include = [&](SlLib::Math::Vector3 const& p) {
            if (!hasPoint)
            {
                min = max = p;
                hasPoint = true;
                return;
            }
            min.X = std::min(min.X, p.X);
            min.Y = std::min(min.Y, p.Y);
            min.Z = std::min(min.Z, p.Z);
            max.X = std::max(max.X, p.X);
            max.Y = std::max(max.Y, p.Y);
            max.Z = std::max(max.Z, p.Z);
        };

        for (auto const& wp : _sifNavigation->Waypoints)
        {
            if (wp)
                include(wp->Pos);
        }

        if (!hasPoint)
        {
            for (auto const& line : _sifNavigation->RacingLines)
            {
                if (!line)
                    continue;
                for (auto const& seg : line->Segments)
                {
                    if (seg)
                        include(seg->RacingLine);
                }
            }
        }

        if (hasPoint)
        {
            std::cout << "[Navigation] bounds min=(" << min.X << ", " << min.Y << ", " << min.Z
                      << ") max=(" << max.X << ", " << max.Y << ", " << max.Z << ")\n";
        }
    }
}

void CharmyBee::UpdateNavigationLineVisibility()
{
    if (_sifNavigationTool == nullptr || _sifNavigation == nullptr)
        return;

    std::vector<std::uint8_t> visibility(_sifNavigation->RacingLines.size(), 1);
    for (auto const& entry : _navigationLineEntries)
    {
        if (entry.LineIndex < 0 || static_cast<std::size_t>(entry.LineIndex) >= visibility.size())
            continue;
        visibility[static_cast<std::size_t>(entry.LineIndex)] = entry.Visible ? 1 : 0;
    }

    _sifNavigationTool->SetRacingLineVisibility(std::move(visibility));
}

void CharmyBee::UpdateNavigationDebugLines()
{
    std::vector<Renderer::SlRenderer::DebugLine> lines;
    if (_drawNavigationWaypoints)
    {
        float half = std::max(0.1f, _navigationWaypointBoxSize * 0.5f);
        const SlLib::Math::Vector3 color{1.0f, 0.0f, 0.0f};
        for (auto const& waypoint : _sifNavigation->Waypoints)
        {
            if (!waypoint)
                continue;
            SlLib::Math::Vector3 c = waypoint->Pos;
            SlLib::Math::Vector3 v0{c.X - half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v1{c.X + half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v2{c.X + half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v3{c.X - half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v4{c.X - half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v5{c.X + half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v6{c.X + half, c.Y + half, c.Z + half};
            SlLib::Math::Vector3 v7{c.X - half, c.Y + half, c.Z + half};

            auto add = [&](SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b) {
                lines.push_back({a, b, color});
            };

            add(v0, v1); add(v1, v2); add(v2, v3); add(v3, v0);
            add(v4, v5); add(v5, v6); add(v6, v7); add(v7, v4);
            add(v0, v4); add(v1, v5); add(v2, v6); add(v3, v7);
        }
    }

    _renderer.SetDebugLines(std::move(lines));
    _renderer.SetDrawDebugLines(true);
}

void CharmyBee::LoadLogicResources()
{
    if (_sifChunks.empty())
    {
        ReportSifError("No SIF chunks loaded.");
        return;
    }

    auto logic = std::make_unique<SlLib::SumoTool::Siff::LogicData>();
    LogicProbeInfo probe{};
    std::string error;
    if (!LoadLogicFromSifChunks(_sifChunks, *logic, probe, error))
    {
        ReportSifError(error);
        return;
    }

    std::cout << "[Logic] base=0x" << std::hex << probe.BaseOffset
              << " version=" << std::dec << probe.Version
              << " triggers=" << probe.NumTriggers
              << " locators=" << probe.NumLocators
              << std::endl;

    _sifLogic = std::move(logic);
    _drawLogic = true;
    LoadItemsForestResources();
    BuildLogicLocatorMeshes();
    UpdateForestMeshRendering();
    UpdateDebugLines();

    _logicTriggerGroups.clear();
    _logicTriggerGroupIndex.clear();
    if (_sifLogic)
    {
        int triggerIndex = 0;
        for (auto const& trigger : _sifLogic->Triggers)
        {
            if (!trigger)
            {
                ++triggerIndex;
                continue;
            }
            int hash = trigger->NameHash;
            auto it = _logicTriggerGroupIndex.find(hash);
            if (it == _logicTriggerGroupIndex.end())
            {
                LogicTriggerGroup group;
                group.Hash = hash;
                group.Name = DescribeTriggerHash(hash);
                group.Count = 1;
                group.Visible = true;
                group.Indices.push_back(triggerIndex);
                _logicTriggerGroups.push_back(std::move(group));
                _logicTriggerGroupIndex.emplace(hash, _logicTriggerGroups.size() - 1);
            }
            else
            {
                _logicTriggerGroups[it->second].Count += 1;
                _logicTriggerGroups[it->second].Indices.push_back(triggerIndex);
            }
            ++triggerIndex;
        }
        std::sort(_logicTriggerGroups.begin(), _logicTriggerGroups.end(),
                  [](auto const& a, auto const& b) { return a.Name < b.Name; });
        _logicTriggerGroupIndex.clear();
        for (std::size_t i = 0; i < _logicTriggerGroups.size(); ++i)
            _logicTriggerGroupIndex.emplace(_logicTriggerGroups[i].Hash, i);
    }
}

void CharmyBee::LoadItemsForestResources()
{
    _itemsForestLibrary.reset();
    _itemsForestMeshesByForestTree.clear();
    _itemsForestMeshesByTreeHash.clear();

    std::span<const std::uint8_t> gpuData;
    if (!_sifGpuRaw.empty())
        gpuData = std::span<const std::uint8_t>(_sifGpuRaw.data(), _sifGpuRaw.size());

    bool loadedFromSif = false;
    std::size_t forestChunkCount = 0;
    for (auto const& chunk : _sifChunks)
    {
        if (chunk.TypeValue != MakeTypeCode('F', 'O', 'R', 'E'))
            continue;
        ++forestChunkCount;

        std::shared_ptr<SeEditor::Forest::ForestLibrary> library;
        std::string error;
        if (!TryLoadForestLibraryFromChunk(chunk, gpuData, library, error))
        {
            std::cerr << "[Logic] Failed to load Forest chunk: " << error << '\n';
            continue;
        }

        std::unordered_map<std::uint64_t, std::shared_ptr<ForestMeshList>> byForestTree;
        std::unordered_map<int, std::shared_ptr<ForestMeshList>> byTreeHash;
        BuildForestTreeMeshMaps(*library, chunk.BigEndian, byForestTree, byTreeHash);

        for (auto& [key, value] : byForestTree)
        {
            if (_itemsForestMeshesByForestTree.find(key) == _itemsForestMeshesByForestTree.end())
                _itemsForestMeshesByForestTree.emplace(key, value);
        }
        for (auto& [key, value] : byTreeHash)
        {
            if (_itemsForestMeshesByTreeHash.find(key) == _itemsForestMeshesByTreeHash.end())
                _itemsForestMeshesByTreeHash.emplace(key, value);
        }

        if (!_itemsForestLibrary)
            _itemsForestLibrary = library;
        loadedFromSif = true;
    }

    if (loadedFromSif)
    {
        std::cout << "[Logic] Loaded " << forestChunkCount << " Forest chunk(s) from SIF. "
                  << "Tree meshes=" << _itemsForestMeshesByTreeHash.size() << std::endl;
        return;
    }

    if (_sifFilePath.empty())
        return;

    std::filesystem::path basePath = std::filesystem::path(_sifFilePath).parent_path();
    std::vector<std::filesystem::path> candidates = {
        basePath / "items.Forest",
        basePath / "items.forest",
        std::filesystem::current_path() / "items.Forest",
        std::filesystem::current_path() / "items.forest",
    };

    std::filesystem::path itemsPath;
    for (auto const& candidate : candidates)
    {
        if (std::filesystem::exists(candidate))
        {
            itemsPath = candidate;
            break;
        }
    }

    if (itemsPath.empty())
        return;

    std::ifstream input(itemsPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "[Logic] Unable to open items.Forest at " << itemsPath << '\n';
        return;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), {});
    if (buffer.empty())
    {
        std::cerr << "[Logic] items.Forest was empty.\n";
        return;
    }

    std::vector<std::uint8_t> rawData(buffer.begin(), buffer.end());
    std::vector<std::uint8_t> cpuData;
    std::vector<std::uint32_t> relocationOffsets;
    std::vector<std::uint8_t> forestGpuData;
    bool bigEndian = false;
    if (!TryParseForestArchive(std::span<const std::uint8_t>(rawData.data(), rawData.size()),
                               cpuData,
                               relocationOffsets,
                               forestGpuData,
                               bigEndian))
    {
        cpuData = std::move(rawData);
        relocationOffsets.clear();
        forestGpuData.clear();
        bigEndian = false;
    }

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(relocationOffsets.size());
    for (auto offset : relocationOffsets)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        cpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(cpuData.data(), cpuData.size()),
        forestGpuData.empty() ? std::span<const std::uint8_t>()
                              : std::span<const std::uint8_t>(forestGpuData.data(), forestGpuData.size()),
        std::move(relocations));
    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_ps3("ps3", true, false, 0);
    context.Platform = bigEndian ? &s_ps3 : &s_win32;

    auto library = std::make_shared<SeEditor::Forest::ForestLibrary>();
    try
    {
        library->Load(context);
    }
    catch (std::exception const& ex)
    {
        std::cerr << "[Logic] Failed to load items.Forest: " << ex.what() << '\n';
        return;
    }

    BuildForestTreeMeshMaps(*library, bigEndian, _itemsForestMeshesByForestTree, _itemsForestMeshesByTreeHash);
    _itemsForestLibrary = std::move(library);
    std::cout << "[Logic] Loaded items.Forest tree meshes: " << _itemsForestMeshesByTreeHash.size() << std::endl;
}

std::vector<std::uint8_t> CharmyBee::BuildLogicChunkData() const
{
    if (_sifLogic == nullptr)
        return {};

    const int numTriggers = static_cast<int>(_sifLogic->Triggers.size());
    const int numLocators = static_cast<int>(_sifLogic->Locators.size());
    const int numAttributes = static_cast<int>(_sifLogic->Attributes.size());

    const std::size_t headerSize = 0x20;
    std::size_t cursor = headerSize;

    std::size_t triggersOffset = 0;
    if (numTriggers > 0)
    {
        triggersOffset = AlignUp(cursor, 0x10);
        cursor = triggersOffset + static_cast<std::size_t>(numTriggers) * 0x70;
    }

    std::size_t attributesOffset = 0;
    if (numAttributes > 0)
    {
        attributesOffset = AlignUp(cursor, 4);
        cursor = attributesOffset + static_cast<std::size_t>(numAttributes) * 0x8;
    }

    std::size_t locatorsOffset = 0;
    if (numLocators > 0)
    {
        locatorsOffset = AlignUp(cursor, 0x10);
        cursor = locatorsOffset + static_cast<std::size_t>(numLocators) * 0x50;
    }

    std::size_t totalSize = AlignUp(cursor, 0x10);
    std::vector<std::uint8_t> out(totalSize, 0);

    WriteInt32LE(out, 0x0, _sifLogic->NameHash);
    WriteInt32LE(out, 0x4, _sifLogic->LogicVersion);
    WriteInt32LE(out, 0x8, numTriggers);
    WriteInt32LE(out, 0xC, numLocators);
    WriteInt32LE(out, 0x10, numTriggers > 0 ? static_cast<std::int32_t>(triggersOffset) : 0);
    WriteInt32LE(out, 0x14, numAttributes > 0 ? static_cast<std::int32_t>(attributesOffset) : 0);
    WriteInt32LE(out, 0x18, numLocators > 0 ? static_cast<std::int32_t>(locatorsOffset) : 0);

    auto writeVec4 = [&](std::size_t offset, SlLib::Math::Vector4 const& v) {
        WriteFloatLE(out, offset + 0x0, v.X);
        WriteFloatLE(out, offset + 0x4, v.Y);
        WriteFloatLE(out, offset + 0x8, v.Z);
        WriteFloatLE(out, offset + 0xC, v.W);
    };

    if (numTriggers > 0 && triggersOffset != 0)
    {
        std::size_t offset = triggersOffset;
        for (auto const& trigger : _sifLogic->Triggers)
        {
            if (trigger)
            {
                WriteInt32LE(out, offset + 0x0, trigger->NameHash);
                WriteInt32LE(out, offset + 0x4, trigger->NumAttributes);
                WriteInt32LE(out, offset + 0x8, trigger->AttributeStartIndex);
                WriteInt32LE(out, offset + 0xC, trigger->Flags);
                writeVec4(offset + 0x10, trigger->Position);
                writeVec4(offset + 0x20, trigger->Normal);
                writeVec4(offset + 0x30, trigger->Vertex0);
                writeVec4(offset + 0x40, trigger->Vertex1);
                writeVec4(offset + 0x50, trigger->Vertex2);
                writeVec4(offset + 0x60, trigger->Vertex3);
            }
            offset += 0x70;
        }
    }

    if (numAttributes > 0 && attributesOffset != 0)
    {
        std::size_t offset = attributesOffset;
        for (auto const& attr : _sifLogic->Attributes)
        {
            if (attr)
            {
                WriteInt32LE(out, offset + 0x0, attr->NameHash);
                WriteInt32LE(out, offset + 0x4, attr->PackedValue);
            }
            offset += 0x8;
        }
    }

    if (numLocators > 0 && locatorsOffset != 0)
    {
        std::size_t offset = locatorsOffset;
        for (auto const& locator : _sifLogic->Locators)
        {
            if (locator)
            {
                WriteInt32LE(out, offset + 0x0, locator->GroupNameHash);
                WriteInt32LE(out, offset + 0x4, locator->LocatorNameHash);
                WriteInt32LE(out, offset + 0x8, locator->MeshForestNameHash);
                WriteInt32LE(out, offset + 0xC, locator->MeshTreeNameHash);
                WriteInt32LE(out, offset + 0x10, locator->SetupObjectNameHash);
                WriteInt32LE(out, offset + 0x14, locator->AnimatedInstanceNameHash);
                WriteInt32LE(out, offset + 0x18, locator->SubDataHash);
                WriteInt32LE(out, offset + 0x1C, locator->Flags);
                WriteInt32LE(out, offset + 0x20, locator->Health);
                WriteFloatLE(out, offset + 0x24, locator->SequenceStartFrameMultiplier);
                WriteFloatLE(out, offset + 0x28, locator->SequencerInterSpawnMultiplier);
                WriteFloatLE(out, offset + 0x2C, locator->AnimatedInstancePlaybackSpeed);
                writeVec4(offset + 0x30, locator->PositionAsFloats);
                writeVec4(offset + 0x40, locator->RotationAsFloats);
            }
            offset += 0x50;
        }
    }

    return out;
}

void CharmyBee::ExportLogicRewrite()
{
    if (_sifLogic == nullptr || _sifChunks.empty())
    {
        ReportSifError("No logic data loaded.");
        return;
    }

    auto logicData = BuildLogicChunkData();
    if (logicData.empty())
    {
        ReportSifError("Failed to build LOGC data.");
        return;
    }

    auto chunks = _sifChunks;
    const std::uint32_t logicType = MakeTypeCode('L', 'O', 'G', 'C');
    for (auto& chunk : chunks)
    {
        if (chunk.TypeValue != logicType)
            continue;
        chunk.Data = logicData;
        chunk.DataSize = static_cast<std::uint32_t>(logicData.size());
        chunk.Relocations.clear();
        if (logicData.size() >= 0x20)
        {
            if (ReadInt32LE(logicData.data() + 0x10) != 0)
                chunk.Relocations.push_back(0x10);
            if (ReadInt32LE(logicData.data() + 0x14) != 0)
                chunk.Relocations.push_back(0x14);
            if (ReadInt32LE(logicData.data() + 0x18) != 0)
                chunk.Relocations.push_back(0x18);
        }
        break;
    }

    std::size_t totalSize = 0;
    for (auto const& chunk : chunks)
    {
        std::size_t minDataSize = 0x10 + chunk.Data.size();
        std::size_t dataChunkSize = chunk.ChunkSize >= minDataSize
            ? chunk.ChunkSize
            : AlignUp(minDataSize, 0x10);
        std::size_t relDataSize = 0x4 + (chunk.Relocations.size() + 1) * 0x8;
        std::size_t minRelSize = 0x10 + relDataSize;
        std::size_t relChunkSize = chunk.RelocChunkSize >= minRelSize
            ? chunk.RelocChunkSize
            : AlignUp(minRelSize, 0x10);
        totalSize += dataChunkSize + relChunkSize;
    }

    std::vector<std::uint8_t> output(totalSize, 0);
    std::size_t cursor = 0;
    for (auto const& chunk : chunks)
    {
        std::size_t minDataSize = 0x10 + chunk.Data.size();
        std::size_t dataChunkSize = chunk.ChunkSize >= minDataSize
            ? chunk.ChunkSize
            : AlignUp(minDataSize, 0x10);
        std::size_t relDataSize = 0x4 + (chunk.Relocations.size() + 1) * 0x8;
        std::size_t minRelSize = 0x10 + relDataSize;
        std::size_t relChunkSize = chunk.RelocChunkSize >= minRelSize
            ? chunk.RelocChunkSize
            : AlignUp(minRelSize, 0x10);

        if (chunk.RawChunk.size() == dataChunkSize)
            std::memcpy(output.data() + cursor, chunk.RawChunk.data(), chunk.RawChunk.size());

        WriteInt32LE(output, cursor + 0x0, static_cast<std::int32_t>(chunk.TypeValue));
        WriteInt32LE(output, cursor + 0x4, static_cast<std::int32_t>(dataChunkSize));
        WriteInt32LE(output, cursor + 0x8, static_cast<std::int32_t>(chunk.Data.size()));
        WriteInt32LE(output, cursor + 0xC, 0x44332211);
        if (!chunk.Data.empty())
            std::memcpy(output.data() + cursor + 0x10, chunk.Data.data(), chunk.Data.size());

        cursor += dataChunkSize;

        if (chunk.RelocRaw.size() == relChunkSize)
            std::memcpy(output.data() + cursor, chunk.RelocRaw.data(), chunk.RelocRaw.size());

        WriteInt32LE(output, cursor + 0x0, static_cast<std::int32_t>(SifResourceType::Relocations));
        WriteInt32LE(output, cursor + 0x4, static_cast<std::int32_t>(relChunkSize));
        WriteInt32LE(output, cursor + 0x8, static_cast<std::int32_t>(relDataSize));
        WriteInt32LE(output, cursor + 0xC, 0x44332211);
        WriteInt32LE(output, cursor + 0x10, static_cast<std::int32_t>(chunk.TypeValue));
        for (std::size_t i = 0; i < chunk.Relocations.size(); ++i)
        {
            std::size_t entry = cursor + 0x10 + 0x4 + i * 0x8;
            output[entry + 0] = 1;
            output[entry + 1] = 0;
            output[entry + 2] = 0;
            output[entry + 3] = 0;
            WriteInt32LE(output, entry + 4, static_cast<std::int32_t>(chunk.Relocations[i]));
        }

        cursor += relChunkSize;
    }

    std::filesystem::path exportRoot = GetStuffRoot() / "export";
    std::error_code ec;
    std::filesystem::create_directories(exportRoot, ec);

    std::filesystem::path outPath;
    if (!_sifFilePath.empty())
    {
        outPath = std::filesystem::path(_sifFilePath).filename();
        outPath.replace_extension(".sif");
    }
    else
    {
        outPath = "logic_rewrite.sif";
    }
    outPath = exportRoot / outPath;

    std::ofstream out(outPath, std::ios::binary);
    if (!out)
    {
        ReportSifError("Unable to write export file.");
        return;
    }
    out.write(reinterpret_cast<const char*>(output.data()), static_cast<std::streamsize>(output.size()));
    out.close();

    std::string zifError;
    std::vector<std::uint8_t> zifData;
    if (Xpac::EncodeZifZig(std::span<const std::uint8_t>(output.data(), output.size()), zifData, zifError))
    {
        std::filesystem::path zifPath = outPath;
        zifPath.replace_extension(".zif");
        std::ofstream zifOut(zifPath, std::ios::binary);
        if (zifOut)
        {
            zifOut.write(reinterpret_cast<const char*>(zifData.data()),
                         static_cast<std::streamsize>(zifData.size()));
            zifOut.close();
        }
    }

    std::cout << "[Logic] Exported LOGC rewrite to " << outPath.string() << std::endl;
}

void CharmyBee::BuildLogicLocatorMeshes()
{
    _logicLocatorMeshes.clear();
    _logicLocatorHasMesh.clear();

    if (_sifLogic == nullptr || _sifLogic->Locators.empty())
        return;

    _logicLocatorHasMesh.resize(_sifLogic->Locators.size(), false);
    if (_itemsForestMeshesByForestTree.empty() && _itemsForestMeshesByTreeHash.empty())
        return;

    auto findMeshList = [&](SlLib::SumoTool::Siff::Logic::Locator const& locator)
        -> std::shared_ptr<ForestMeshList> {
        auto tryTreeHash = [&](int treeHash) -> std::shared_ptr<ForestMeshList> {
            if (treeHash == 0)
                return nullptr;
            auto treeIt = _itemsForestMeshesByTreeHash.find(treeHash);
            if (treeIt != _itemsForestMeshesByTreeHash.end())
                return treeIt->second;
            return nullptr;
        };

        if (locator.MeshForestNameHash != 0 && locator.MeshTreeNameHash != 0)
        {
            auto key = MakeForestTreeKey(locator.MeshForestNameHash, locator.MeshTreeNameHash);
            auto it = _itemsForestMeshesByForestTree.find(key);
            if (it != _itemsForestMeshesByForestTree.end())
                return it->second;
        }

        if (auto mesh = tryTreeHash(locator.MeshTreeNameHash))
            return mesh;
        if (auto mesh = tryTreeHash(locator.SetupObjectNameHash))
            return mesh;
        if (auto mesh = tryTreeHash(locator.MeshForestNameHash))
            return mesh;

        return nullptr;
    };

    std::size_t matched = 0;
    for (std::size_t i = 0; i < _sifLogic->Locators.size(); ++i)
    {
        auto const& locator = _sifLogic->Locators[i];
        auto meshList = findMeshList(*locator);
        if (!meshList)
            continue;

        _logicLocatorHasMesh[i] = true;
        ++matched;

        SlLib::Math::Quaternion q{locator->RotationAsFloats.X,
                                  locator->RotationAsFloats.Y,
                                  locator->RotationAsFloats.Z,
                                  locator->RotationAsFloats.W};
        float qLen = std::sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
        if (qLen > 0.0f)
        {
            float inv = 1.0f / qLen;
            q = q * inv;
        }
        else
        {
            q = {0.0f, 0.0f, 0.0f, 1.0f};
        }

        SlLib::Math::Matrix4x4 rot = SlLib::Math::CreateFromQuaternion(q);
        SlLib::Math::Matrix4x4 world = rot;
        world(0, 3) = locator->PositionAsFloats.X;
        world(1, 3) = locator->PositionAsFloats.Y;
        world(2, 3) = locator->PositionAsFloats.Z;
        world(3, 3) = 1.0f;

        for (auto const& mesh : *meshList)
        {
            Renderer::SlRenderer::ForestCpuMesh transformed = mesh;
            transformed.Model = IdentityMatrix();
            transformed.Skinned = false;
            transformed.BoneMatrixIndices.clear();
            transformed.BoneInverseMatrices.clear();
            transformed.BonePalette.clear();
            for (std::size_t v = 0; v + 15 < transformed.Vertices.size(); v += 16)
            {
                SlLib::Math::Vector4 pos{
                    transformed.Vertices[v + 0],
                    transformed.Vertices[v + 1],
                    transformed.Vertices[v + 2],
                    1.0f};
                SlLib::Math::Vector4 nrm{
                    transformed.Vertices[v + 3],
                    transformed.Vertices[v + 4],
                    transformed.Vertices[v + 5],
                    0.0f};

                auto p = SlLib::Math::Transform(world, pos);
                auto n = SlLib::Math::Transform(rot, nrm);
                auto n3 = SlLib::Math::normalize({n.X, n.Y, n.Z});

                transformed.Vertices[v + 0] = p.X;
                transformed.Vertices[v + 1] = p.Y;
                transformed.Vertices[v + 2] = p.Z;
                transformed.Vertices[v + 3] = n3.X;
                transformed.Vertices[v + 4] = n3.Y;
                transformed.Vertices[v + 5] = n3.Z;
            }

            _logicLocatorMeshes.push_back(std::move(transformed));
        }
    }

    std::cout << "[Logic] Locator meshes resolved: " << matched << " / " << _sifLogic->Locators.size()
              << " (trees=" << _itemsForestMeshesByTreeHash.size()
              << " forestTrees=" << _itemsForestMeshesByForestTree.size() << ")\n";
}

void CharmyBee::UpdateLogicDebugLines()
{
    std::vector<Renderer::SlRenderer::DebugLine> lines;
    if (_sifLogic == nullptr)
        return;

    if (_drawLogicTriggers)
    {
        const SlLib::Math::Vector3 color{0.2f, 0.9f, 0.9f};
        for (auto const& trigger : _sifLogic->Triggers)
        {
            if (!trigger)
                continue;
            auto v0 = SlLib::Math::Vector3{trigger->Vertex0.X, trigger->Vertex0.Y, trigger->Vertex0.Z};
            auto v1 = SlLib::Math::Vector3{trigger->Vertex1.X, trigger->Vertex1.Y, trigger->Vertex1.Z};
            auto v2 = SlLib::Math::Vector3{trigger->Vertex2.X, trigger->Vertex2.Y, trigger->Vertex2.Z};
            auto v3 = SlLib::Math::Vector3{trigger->Vertex3.X, trigger->Vertex3.Y, trigger->Vertex3.Z};
            lines.push_back(Renderer::SlRenderer::DebugLine{v0, v1, color});
            lines.push_back(Renderer::SlRenderer::DebugLine{v1, v2, color});
            lines.push_back(Renderer::SlRenderer::DebugLine{v2, v3, color});
            lines.push_back(Renderer::SlRenderer::DebugLine{v3, v0, color});
        }
    }

    if (_drawLogicLocators)
    {
        float half = std::max(0.1f, _logicLocatorBoxSize * 0.5f);
        const SlLib::Math::Vector3 color{0.9f, 0.5f, 0.1f};
        for (std::size_t i = 0; i < _sifLogic->Locators.size(); ++i)
        {
            auto const& locator = _sifLogic->Locators[i];
            if (!locator)
                continue;
            if (i < _logicLocatorHasMesh.size() && _logicLocatorHasMesh[i])
                continue;
            SlLib::Math::Vector3 c{locator->PositionAsFloats.X,
                                   locator->PositionAsFloats.Y,
                                   locator->PositionAsFloats.Z};
            SlLib::Math::Vector3 v0{c.X - half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v1{c.X + half, c.Y - half, c.Z - half};
            SlLib::Math::Vector3 v2{c.X + half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v3{c.X - half, c.Y + half, c.Z - half};
            SlLib::Math::Vector3 v4{c.X - half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v5{c.X + half, c.Y - half, c.Z + half};
            SlLib::Math::Vector3 v6{c.X + half, c.Y + half, c.Z + half};
            SlLib::Math::Vector3 v7{c.X - half, c.Y + half, c.Z + half};

            auto add = [&](SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b) {
                lines.push_back(Renderer::SlRenderer::DebugLine{a, b, color});
            };

            add(v0, v1); add(v1, v2); add(v2, v3); add(v3, v0);
            add(v4, v5); add(v5, v6); add(v6, v7); add(v7, v4);
            add(v0, v4); add(v1, v5); add(v2, v6); add(v3, v7);
        }
    }

    if (!lines.empty())
        _renderer.SetDrawDebugLines(true);

    _renderer.SetDebugLines(std::move(lines));
}

void CharmyBee::UpdateDebugLines()
{
    std::vector<Renderer::SlRenderer::DebugLine> combined;

    if (_drawNavigation && _sifNavigation)
    {
        std::vector<Renderer::SlRenderer::DebugLine> navLines;
        if (_drawNavigationWaypoints)
        {
            float half = std::max(0.1f, _navigationWaypointBoxSize * 0.5f);
            const SlLib::Math::Vector3 color{1.0f, 0.0f, 0.0f};
            for (auto const& waypoint : _sifNavigation->Waypoints)
            {
                if (!waypoint)
                    continue;
                SlLib::Math::Vector3 c = waypoint->Pos;
                SlLib::Math::Vector3 v0{c.X - half, c.Y - half, c.Z - half};
                SlLib::Math::Vector3 v1{c.X + half, c.Y - half, c.Z - half};
                SlLib::Math::Vector3 v2{c.X + half, c.Y + half, c.Z - half};
                SlLib::Math::Vector3 v3{c.X - half, c.Y + half, c.Z - half};
                SlLib::Math::Vector3 v4{c.X - half, c.Y - half, c.Z + half};
                SlLib::Math::Vector3 v5{c.X + half, c.Y - half, c.Z + half};
                SlLib::Math::Vector3 v6{c.X + half, c.Y + half, c.Z + half};
                SlLib::Math::Vector3 v7{c.X - half, c.Y + half, c.Z + half};

                auto add = [&](SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b) {
                    navLines.push_back(Renderer::SlRenderer::DebugLine{a, b, color});
                };

                add(v0, v1); add(v1, v2); add(v2, v3); add(v3, v0);
                add(v4, v5); add(v5, v6); add(v6, v7); add(v7, v4);
                add(v0, v4); add(v1, v5); add(v2, v6); add(v3, v7);
            }
        }

        combined.insert(combined.end(), navLines.begin(), navLines.end());
    }

    if (_drawLogic && _sifLogic)
    {
        std::vector<Renderer::SlRenderer::DebugLine> logicLines;
        if (_drawLogicTriggers)
        {
            const SlLib::Math::Vector3 color{0.2f, 0.9f, 0.9f};
            for (auto const& trigger : _sifLogic->Triggers)
            {
                if (!trigger)
                    continue;
                if (!_logicTriggerGroups.empty())
                {
                    auto it = _logicTriggerGroupIndex.find(trigger->NameHash);
                    if (it != _logicTriggerGroupIndex.end() && !_logicTriggerGroups[it->second].Visible)
                        continue;
                }
                auto v0 = SlLib::Math::Vector3{trigger->Vertex0.X, trigger->Vertex0.Y, trigger->Vertex0.Z};
                auto v1 = SlLib::Math::Vector3{trigger->Vertex1.X, trigger->Vertex1.Y, trigger->Vertex1.Z};
                auto v2 = SlLib::Math::Vector3{trigger->Vertex2.X, trigger->Vertex2.Y, trigger->Vertex2.Z};
                auto v3 = SlLib::Math::Vector3{trigger->Vertex3.X, trigger->Vertex3.Y, trigger->Vertex3.Z};
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v0, v1, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v1, v2, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v2, v3, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v3, v0, color});

                if (_drawLogicTriggerNormals)
                {
                    SlLib::Math::Vector3 n{trigger->Normal.X, trigger->Normal.Y, trigger->Normal.Z};
                    float len = std::sqrt(n.X * n.X + n.Y * n.Y + n.Z * n.Z);
                    if (len <= 1e-4f)
                    {
                        SlLib::Math::Vector3 e0{v1.X - v0.X, v1.Y - v0.Y, v1.Z - v0.Z};
                        SlLib::Math::Vector3 e1{v3.X - v0.X, v3.Y - v0.Y, v3.Z - v0.Z};
                        n = SlLib::Math::cross(e0, e1);
                        len = std::sqrt(n.X * n.X + n.Y * n.Y + n.Z * n.Z);
                    }
                    if (len > 1e-4f)
                    {
                        float inv = 1.0f / len;
                        SlLib::Math::Vector3 dir{n.X * inv, n.Y * inv, n.Z * inv};
                        float size = std::max(0.1f, _logicTriggerNormalSize);
                        SlLib::Math::Vector3 center{trigger->Position.X, trigger->Position.Y, trigger->Position.Z};
                        SlLib::Math::Vector3 end{center.X + dir.X * size,
                                                 center.Y + dir.Y * size,
                                                 center.Z + dir.Z * size};
                        logicLines.push_back(Renderer::SlRenderer::DebugLine{center, end, {0.9f, 0.2f, 0.9f}});
                    }
                }
            }
        }

        if (_drawLogicLocators)
        {
            float half = std::max(0.1f, _logicLocatorBoxSize * 0.5f);
            const SlLib::Math::Vector3 color{0.9f, 0.5f, 0.1f};
            for (auto const& locator : _sifLogic->Locators)
            {
                if (!locator)
                    continue;
                SlLib::Math::Vector3 c{locator->PositionAsFloats.X,
                                       locator->PositionAsFloats.Y,
                                       locator->PositionAsFloats.Z};
                SlLib::Math::Vector3 v0{c.X - half, c.Y - half, c.Z - half};
                SlLib::Math::Vector3 v1{c.X + half, c.Y - half, c.Z - half};
                SlLib::Math::Vector3 v2{c.X + half, c.Y + half, c.Z - half};
                SlLib::Math::Vector3 v3{c.X - half, c.Y + half, c.Z - half};
                SlLib::Math::Vector3 v4{c.X - half, c.Y - half, c.Z + half};
                SlLib::Math::Vector3 v5{c.X + half, c.Y - half, c.Z + half};
                SlLib::Math::Vector3 v6{c.X + half, c.Y + half, c.Z + half};
                SlLib::Math::Vector3 v7{c.X - half, c.Y + half, c.Z + half};

                auto add = [&](SlLib::Math::Vector3 const& a, SlLib::Math::Vector3 const& b) {
                    logicLines.push_back(Renderer::SlRenderer::DebugLine{a, b, color});
                };

                add(v0, v1); add(v1, v2); add(v2, v3); add(v3, v0);
                add(v4, v5); add(v5, v6); add(v6, v7); add(v7, v4);
                add(v0, v4); add(v1, v5); add(v2, v6); add(v3, v7);
            }
        }

        if (_drawLogicLocatorAxes)
        {
            float size = std::max(0.1f, _logicLocatorAxisSize);
            for (auto const& locator : _sifLogic->Locators)
            {
                if (!locator)
                    continue;
                SlLib::Math::Quaternion q{locator->RotationAsFloats.X,
                                          locator->RotationAsFloats.Y,
                                          locator->RotationAsFloats.Z,
                                          locator->RotationAsFloats.W};
                float qLen = std::sqrt(q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W);
                if (qLen > 0.0f)
                {
                    float inv = 1.0f / qLen;
                    q = q * inv;
                }
                else
                {
                    q = {0.0f, 0.0f, 0.0f, 1.0f};
                }

                SlLib::Math::Matrix4x4 rot = SlLib::Math::CreateFromQuaternion(q);
                SlLib::Math::Vector3 x{rot(0, 0), rot(1, 0), rot(2, 0)};
                SlLib::Math::Vector3 y{rot(0, 1), rot(1, 1), rot(2, 1)};
                SlLib::Math::Vector3 z{rot(0, 2), rot(1, 2), rot(2, 2)};

                SlLib::Math::Vector3 c{locator->PositionAsFloats.X,
                                       locator->PositionAsFloats.Y,
                                       locator->PositionAsFloats.Z};
                logicLines.push_back(Renderer::SlRenderer::DebugLine{c, {c.X + x.X * size, c.Y + x.Y * size, c.Z + x.Z * size}, {1.0f, 0.2f, 0.2f}});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{c, {c.X + y.X * size, c.Y + y.Y * size, c.Z + y.Z * size}, {0.2f, 1.0f, 0.2f}});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{c, {c.X + z.X * size, c.Y + z.Y * size, c.Z + z.Z * size}, {0.2f, 0.4f, 1.0f}});
            }
        }

        combined.insert(combined.end(), logicLines.begin(), logicLines.end());
    }

    _renderer.SetDebugLines(std::move(combined));
    _renderer.SetDrawDebugLines(_drawNavigation || _drawLogic);
}

void CharmyBee::RebuildForestBoxHierarchy()
{
    UpdateForestBoxRenderer();
}

void CharmyBee::UpdateForestBoxRenderer()
{
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes;
    boxes.reserve(_forestBoxLayers.size());

    if (!_drawForestBoxes)
    {
        _renderer.SetForestBoxes({});
        _renderer.SetDrawForestBoxes(false);
        return;
    }

    std::function<void(ForestBoxLayer const&)> gather =
        [&](ForestBoxLayer const& layer) {
            if (layer.Visible && layer.HasBounds)
                boxes.emplace_back(layer.Min, layer.Max);
            for (auto const& child : layer.Children)
                gather(child);
        };

    for (auto const& layer : _forestBoxLayers)
        gather(layer);

    _renderer.SetForestBoxes(std::move(boxes));
    _renderer.SetDrawForestBoxes(_drawForestBoxes);
}

void CharmyBee::UpdateTriggerPhantomBoxes()
{
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes;
    if (_database == nullptr)
    {
        if (_sifLogic == nullptr || _sifLogic->Triggers.empty())
        {
            _renderer.SetTriggerBoxes({});
            _renderer.SetDrawTriggerBoxes(false);
            return;
        }

        auto addBoxFromPoints = [&](std::span<SlLib::Math::Vector3 const> points) {
            SlLib::Math::Vector3 min = points.front();
            SlLib::Math::Vector3 max = points.front();
            for (auto const& p : points)
            {
                min.X = std::min(min.X, p.X);
                min.Y = std::min(min.Y, p.Y);
                min.Z = std::min(min.Z, p.Z);
                max.X = std::max(max.X, p.X);
                max.Y = std::max(max.Y, p.Y);
                max.Z = std::max(max.Z, p.Z);
            }
            boxes.emplace_back(min, max);
        };

        for (auto const& trigger : _sifLogic->Triggers)
        {
            if (!trigger)
                continue;
            if (!_logicTriggerGroups.empty())
            {
                auto it = _logicTriggerGroupIndex.find(trigger->NameHash);
                if (it != _logicTriggerGroupIndex.end() && !_logicTriggerGroups[it->second].Visible)
                    continue;
            }

            SlLib::Math::Vector3 v0{trigger->Vertex0.X, trigger->Vertex0.Y, trigger->Vertex0.Z};
            SlLib::Math::Vector3 v1{trigger->Vertex1.X, trigger->Vertex1.Y, trigger->Vertex1.Z};
            SlLib::Math::Vector3 v2{trigger->Vertex2.X, trigger->Vertex2.Y, trigger->Vertex2.Z};
            SlLib::Math::Vector3 v3{trigger->Vertex3.X, trigger->Vertex3.Y, trigger->Vertex3.Z};
            SlLib::Math::Vector3 center{trigger->Position.X, trigger->Position.Y, trigger->Position.Z};
            SlLib::Math::Vector3 normal{trigger->Normal.X, trigger->Normal.Y, trigger->Normal.Z};

            float len = std::sqrt(normal.X * normal.X + normal.Y * normal.Y + normal.Z * normal.Z);
            SlLib::Math::Vector3 dir = (len > 1e-4f)
                ? SlLib::Math::Vector3{normal.X / len, normal.Y / len, normal.Z / len}
                : SlLib::Math::Vector3{0.0f, 1.0f, 0.0f};
            float depth = std::max(1.0f, len);
            SlLib::Math::Vector3 offset{dir.X * depth, dir.Y * depth, dir.Z * depth};

            std::array<SlLib::Math::Vector3, 9> points{
                v0, v1, v2, v3,
                SlLib::Math::Vector3{v0.X + offset.X, v0.Y + offset.Y, v0.Z + offset.Z},
                SlLib::Math::Vector3{v1.X + offset.X, v1.Y + offset.Y, v1.Z + offset.Z},
                SlLib::Math::Vector3{v2.X + offset.X, v2.Y + offset.Y, v2.Z + offset.Z},
                SlLib::Math::Vector3{v3.X + offset.X, v3.Y + offset.Y, v3.Z + offset.Z},
                center
            };

            addBoxFromPoints(points);
        }

        _renderer.SetTriggerBoxes(std::move(boxes));
        _renderer.SetDrawTriggerBoxes(_drawTriggerBoxes);
        return;
    }

    using SlLib::Resources::Scene::SeDefinitionNode;
    using SlLib::Resources::Scene::TriggerPhantomDefinitionNode;

    auto addBox = [&](SlLib::Math::Vector3 const& center, SlLib::Math::Vector3 const& scale) {
        SlLib::Math::Vector3 half{scale.X * 0.5f, scale.Y * 0.5f, scale.Z * 0.5f};
        SlLib::Math::Vector3 min{center.X - half.X, center.Y - half.Y, center.Z - half.Z};
        SlLib::Math::Vector3 max{center.X + half.X, center.Y + half.Y, center.Z + half.Z};
        boxes.emplace_back(min, max);
    };

    std::function<void(SeDefinitionNode*)> walk;
    walk = [&](SeDefinitionNode* node) {
        if (node == nullptr)
            return;

        if (auto* phantom = dynamic_cast<TriggerPhantomDefinitionNode*>(node))
        {
            float sx = phantom->WidthRadius;
            float sy = phantom->Height;
            float sz = phantom->Depth;

            // Normalize missing sizes.
            if (sx <= 0.0f && (sy > 0.0f || sz > 0.0f))
                sx = std::max(sy, sz);
            if (sy <= 0.0f)
                sy = sx;
            if (sz <= 0.0f)
                sz = sx;

            SlLib::Math::Vector3 scale{sx, sy, sz};
            addBox(phantom->Translation, scale);
        }

        for (auto* child = node->FirstChild; child != nullptr; child = child->NextSibling)
        {
            if (auto* def = dynamic_cast<SeDefinitionNode*>(child))
                walk(def);
        }
    };

    for (auto* root : _database->RootDefinitions)
        walk(root);

    _renderer.SetTriggerBoxes(std::move(boxes));
    _renderer.SetDrawTriggerBoxes(_drawTriggerBoxes);
}

void CharmyBee::UpdateForestMeshRendering()
{
    std::vector<Renderer::SlRenderer::ForestCpuMesh> combined;
    std::uint64_t selectionHash = 1469598103934665603ull;
    auto hashMix = [&](std::uint64_t value) {
        selectionHash ^= value + 0x9e3779b97f4a7c15ull + (selectionHash << 6) + (selectionHash >> 2);
    };

    if (_drawForestMeshes && !_allForestMeshes.empty())
    {
        std::vector<Renderer::SlRenderer::ForestCpuMesh> filtered;
        filtered.reserve(_allForestMeshes.size());

        auto gatherMeshes = [&](auto&& self, ForestBoxLayer const& layer) -> void {
            if (!layer.Visible)
                return;

            if (layer.TreeIndex >= 0)
            {
                if (!IsTreeVisible(layer.ForestIndex, layer.TreeIndex))
                    return;
            }

            if (layer.MeshCount > 0 && layer.MeshStartIndex < _allForestMeshes.size())
            {
                std::size_t end = std::min(layer.MeshStartIndex + layer.MeshCount, _allForestMeshes.size());
                filtered.insert(filtered.end(),
                                _allForestMeshes.begin() + layer.MeshStartIndex,
                                _allForestMeshes.begin() + end);
            }

            for (auto const& child : layer.Children)
                self(self, child);
        };

        for (auto const& layer : _forestBoxLayers)
            gatherMeshes(gatherMeshes, layer);

        filtered.erase(std::remove_if(filtered.begin(), filtered.end(),
                                      [this](Renderer::SlRenderer::ForestCpuMesh const& mesh) {
                                          if (!IsBranchVisible(mesh.ForestIndex, mesh.TreeIndex, mesh.BranchIndex))
                                              return true;
                                          if (_animatorVisibilityForest >= 0 &&
                                              _animatorVisibilityTree >= 0 &&
                                              mesh.ForestIndex == _animatorVisibilityForest &&
                                              mesh.TreeIndex == _animatorVisibilityTree &&
                                              mesh.BranchIndex >= 0 &&
                                              static_cast<std::size_t>(mesh.BranchIndex) < _animatorBranchVisibility.size() &&
                                              !_animatorBranchVisibility[static_cast<std::size_t>(mesh.BranchIndex)])
                                              return true;
                                          return false;
                                      }),
                       filtered.end());

        if (!filtered.empty())
        {
            combined.reserve(filtered.size() + _logicLocatorMeshes.size());
            combined.insert(combined.end(), filtered.begin(), filtered.end());
        }

        for (auto const& mesh : filtered)
        {
            hashMix(static_cast<std::uint64_t>(mesh.ForestIndex));
            hashMix(static_cast<std::uint64_t>(mesh.TreeIndex));
            hashMix(static_cast<std::uint64_t>(mesh.BranchIndex));
        }
    }

    if (_drawLogic && _drawLogicLocators && !_logicLocatorMeshes.empty())
    {
        if (combined.empty())
            combined.reserve(_logicLocatorMeshes.size());
        combined.insert(combined.end(), _logicLocatorMeshes.begin(), _logicLocatorMeshes.end());
        hashMix(0x10C1C0u);
        hashMix(static_cast<std::uint64_t>(_logicLocatorMeshes.size()));
    }

    bool hasVisible = !combined.empty();
    if (selectionHash == _forestRenderHash &&
        _forestRenderCount == combined.size() &&
        _forestRenderVisible == hasVisible)
    {
        _renderer.SetDrawForestMeshes(hasVisible);
        return;
    }

    _forestRenderHash = selectionHash;
    _forestRenderCount = combined.size();
    _forestRenderVisible = hasVisible;
    _renderer.SetForestMeshes(std::move(combined));
    _renderer.SetDrawForestMeshes(hasVisible);

    if (std::getenv("RENDER_PRE_DEBUG") != nullptr)
    {
        static auto s_last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - s_last >= std::chrono::seconds(1))
        {
            s_last = now;
            std::size_t meshCount = _renderer.DebugForestMeshCount();
            std::size_t skinnedCount = _renderer.DebugForestSkinnedCount();
            std::size_t dirty = _renderer.DebugForestDirty() ? 1u : 0u;
            std::cout << "[RenderPre] meshes=" << meshCount
                      << " skinned=" << skinnedCount
                      << " dirty=" << dirty
                      << " draw=" << (hasVisible ? 1 : 0)
                      << std::endl;
        }
    }
}

void CharmyBee::UpdateForestHierarchy()
{
    _forestHierarchy.clear();
    if (!_forestLibrary)
        return;

    for (std::size_t forestIdx = 0; forestIdx < _forestLibrary->Forests.size(); ++forestIdx)
    {
        auto const& forestEntry = _forestLibrary->Forests[forestIdx];
        ForestHierarchy hierarchy;
        hierarchy.Name = forestEntry.Name.empty()
                         ? std::string("Forest ") + std::to_string(forestEntry.Hash)
                         : forestEntry.Name;
        hierarchy.Visible = true;
            if (!forestEntry.Forest)
            {
                _forestHierarchy.push_back(std::move(hierarchy));
                continue;
            }

        for (std::size_t treeIdx = 0; treeIdx < forestEntry.Forest->Trees.size(); ++treeIdx)
        {
            auto const& tree = forestEntry.Forest->Trees[treeIdx];
            if (!tree)
                continue;

            TreeHierarchy treeHierarchy;
            treeHierarchy.Name = tree->Hash != 0
                                     ? std::string("Tree ") + std::to_string(tree->Hash)
                                     : std::string("Tree ") + std::to_string(treeIdx);
            treeHierarchy.Visible = true;
            treeHierarchy.ForestIndex = static_cast<int>(forestIdx);
            treeHierarchy.TreeIndex = static_cast<int>(treeIdx);
            std::size_t branchCount = tree->Branches.size();
            treeHierarchy.Nodes.resize(branchCount);
            for (std::size_t i = 0; i < branchCount; ++i)
                treeHierarchy.Nodes[i].Branch = tree->Branches[i];

            for (auto& node : treeHierarchy.Nodes)
                node.Visible = true;

            for (int i = 0; i < static_cast<int>(branchCount); ++i)
            {
                auto const& branch = treeHierarchy.Nodes[static_cast<std::size_t>(i)].Branch;
                if (!branch)
                    continue;

                int child = branch->Child;
                while (child >= 0 && static_cast<std::size_t>(child) < branchCount)
                {
                    treeHierarchy.Nodes[static_cast<std::size_t>(i)].Children.push_back(child);
                    auto const& childBranch = treeHierarchy.Nodes[static_cast<std::size_t>(child)].Branch;
                    if (!childBranch)
                        break;
                    child = childBranch->Sibling;
                }

                if (branch->Parent < 0)
                    treeHierarchy.Roots.push_back(i);
            }

            if (treeHierarchy.Roots.empty())
            {
                for (std::size_t i = 0; i < branchCount; ++i)
                {
                    if (treeHierarchy.Nodes[i].Branch)
                    {
                        treeHierarchy.Roots.push_back(static_cast<int>(i));
                        break;
                    }
                }
            }

            std::cout << "[ForestHierarchy] tree=" << treeHierarchy.Name << " branches=" << branchCount
                      << " roots=" << treeHierarchy.Roots.size() << std::endl;
            bool dumpAll = (std::getenv("FOREST_HIER_DUMP") != nullptr);
            std::size_t limit = dumpAll ? branchCount : std::min<std::size_t>(5, branchCount);
            for (std::size_t idx = 0; idx < limit; ++idx)
            {
                auto const& branch = treeHierarchy.Nodes[idx].Branch;
                std::string name = branch && !branch->Name.empty()
                                       ? branch->Name
                                       : std::string("unnamed");
                std::cout << "  branch[" << idx << "] parent=" << (branch ? branch->Parent : -1)
                          << " child=" << (branch ? branch->Child : -1)
                          << " sibling=" << (branch ? branch->Sibling : -1)
                          << " name=" << name << std::endl;
            }

            if (treeHierarchy.Roots.empty())
            {
                for (std::size_t i = 0; i < branchCount; ++i)
                {
                    if (treeHierarchy.Nodes[i].Branch)
                    {
                        treeHierarchy.Roots.push_back(static_cast<int>(i));
                        break;
                    }
                }
            }

            hierarchy.Trees.push_back(std::move(treeHierarchy));
        }

        _forestHierarchy.push_back(std::move(hierarchy));
    }
}

bool CharmyBee::IsTreeVisible(std::size_t forestIdx, std::size_t treeIdx) const
{
    if (forestIdx >= _forestHierarchy.size())
        return true;
    auto const& forest = _forestHierarchy[forestIdx];
    if (treeIdx >= forest.Trees.size())
        return true;
    return forest.Visible && forest.Trees[treeIdx].Visible;
}

bool CharmyBee::IsBranchVisible(int forestIdx, int treeIdx, int branchIdx) const
{
    if (branchIdx < 0 || forestIdx < 0 || treeIdx < 0)
        return true;
    if (static_cast<std::size_t>(forestIdx) >= _forestHierarchy.size())
        return true;
    auto const& forest = _forestHierarchy[static_cast<std::size_t>(forestIdx)];
    if (!forest.Visible)
        return false;
    if (static_cast<std::size_t>(treeIdx) >= forest.Trees.size())
        return true;
    auto const& tree = forest.Trees[static_cast<std::size_t>(treeIdx)];
    if (!tree.Visible)
        return false;

    int current = branchIdx;
    while (current >= 0 && static_cast<std::size_t>(current) < tree.Nodes.size())
    {
        auto const& node = tree.Nodes[static_cast<std::size_t>(current)];
        if (!node.Visible)
            return false;
        if (!node.Branch)
            break;
        current = node.Branch->Parent;
    }
    return true;
}

void CharmyBee::ApplyTreeVisibilityToLayers()
{
    if (_forestBoxLayers.empty())
        return;

    for (auto& layer : _forestBoxLayers)
    {
        if (layer.ForestIndex >= 0 && static_cast<std::size_t>(layer.ForestIndex) < _forestHierarchy.size())
            layer.Visible = _forestHierarchy[layer.ForestIndex].Visible;

        for (auto& child : layer.Children)
        {
            if (child.ForestIndex >= 0 && child.TreeIndex >= 0)
            {
                child.Visible = IsTreeVisible(static_cast<std::size_t>(child.ForestIndex),
                                               static_cast<std::size_t>(child.TreeIndex));
            }
        }
    }
}

bool CharmyBee::RenderBranchNode(CharmyBee::TreeHierarchy& tree, int index)
{
    if (index < 0 || static_cast<std::size_t>(index) >= tree.Nodes.size())
        return false;

    auto& node = tree.Nodes[static_cast<std::size_t>(index)];
    std::string label = node.Branch && !node.Branch->Name.empty()
                            ? node.Branch->Name
                            : std::string("Branch ") + std::to_string(index);
    std::string unique = label + "##branch_" + std::to_string(index);
    std::string checkboxId = unique + "_chk";

    bool changed = ImGui::Checkbox(checkboxId.c_str(), &node.Visible);
    ImGui::SameLine();
    if (node.Children.empty())
    {
        ImGui::Text("%s", label.c_str());
    }
    else if (ImGui::TreeNode(unique.c_str(), "%s", label.c_str()))
    {
        for (int child : node.Children)
            changed |= RenderBranchNode(tree, child);
        ImGui::TreePop();
    }
    return changed;
}

void CharmyBee::RenderForestHierarchyWindow()
{
    if (!_showForestHierarchyWindow)
        return;

    if (!ImGui::Begin("Forest hierarchy", &_showForestHierarchyWindow))
    {
        ImGui::End();
        return;
    }
    if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        _blockSceneInput = true;

    if (_forestHierarchy.empty())
    {
        ImGui::TextDisabled("No forest hierarchy available.");
        ImGui::End();
        return;
    }

    ImGui::BeginChild("ForestHierarchyContent", ImVec2(0.0f, 0.0f), true);
    RenderForestHierarchyList();
    ImGui::EndChild();

    ImGui::End();
}

void CharmyBee::RenderForestHierarchyList()
{
    bool hierarchyChanged = false;

    for (auto& forest : _forestHierarchy)
    {
        std::string label = forest.Name.empty() ? "Forest" : forest.Name;
        std::string forestId = label + "##forest_" + std::to_string(reinterpret_cast<std::uintptr_t>(&forest));
        bool forestChanged = ImGui::Checkbox(forestId.c_str(), &forest.Visible);
        hierarchyChanged |= forestChanged;
        if (forestChanged)
        {
            for (auto& tree : forest.Trees)
                tree.Visible = forest.Visible;
        }
        ImGui::SameLine();
        if (!forest.Visible)
        {
            ImGui::TextDisabled("%s", label.c_str());
            continue;
        }
        if (ImGui::CollapsingHeader(label.c_str()))
        {
            for (std::size_t treeIdx = 0; treeIdx < forest.Trees.size(); ++treeIdx)
            {
                auto& tree = forest.Trees[treeIdx];
                std::string treeLabel = tree.Name.empty()
                                            ? std::string("Tree ") + std::to_string(treeIdx)
                                            : tree.Name;
                std::string treeId = treeLabel + "##tree_" + std::to_string(treeIdx);
                std::string treeChk = treeId + "_chk";
                bool treeChanged = ImGui::Checkbox(treeChk.c_str(), &tree.Visible);
                hierarchyChanged |= treeChanged;
                ImGui::SameLine();
                if (!tree.Visible)
                {
                    ImGui::TextDisabled("%s", treeLabel.c_str());
                    continue;
                }
                if (ImGui::TreeNodeEx(treeId.c_str(),
                                      ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen,
                                      "%s",
                                      treeLabel.c_str()))
                {
                    if (tree.Roots.empty())
                    {
                        ImGui::TextDisabled("No branch data available.");
                    }
                    else
                    {
                        for (int root : tree.Roots)
                            hierarchyChanged |= RenderBranchNode(tree, root);
                    }
                    ImGui::TreePop();
                }
            }
        }
    }

    if (hierarchyChanged)
    {
        ApplyTreeVisibilityToLayers();
        UpdateForestBoxRenderer();
        UpdateForestMeshRendering();
        ApplyAnimatorFrame();
        SaveForestHierarchyVisibility();
    }
}

void CharmyBee::BuildForestMeshesFromPose(std::vector<SlLib::Math::Vector4> const& translations,
                                          std::vector<SlLib::Math::Vector4> const& rotations,
                                          std::vector<SlLib::Math::Vector4> const& scales,
                                          int forestIndex,
                                          int treeIndex)
{
    if (!_forestLibrary)
        return;
    if (forestIndex < 0 || treeIndex < 0)
        return;
    if (static_cast<std::size_t>(forestIndex) >= _forestLibrary->Forests.size())
        return;

    auto const& forestEntry = _forestLibrary->Forests[static_cast<std::size_t>(forestIndex)];
    if (!forestEntry.Forest)
        return;
    if (static_cast<std::size_t>(treeIndex) >= forestEntry.Forest->Trees.size())
        return;

    auto const& tree = forestEntry.Forest->Trees[static_cast<std::size_t>(treeIndex)];
    if (!tree)
        return;

    std::size_t branchCount = tree->Branches.size();
    std::vector<SlLib::Math::Matrix4x4> world(branchCount);
    std::vector<bool> computed(branchCount, false);

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

    std::function<SlLib::Math::Matrix4x4(int)> computeWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
        if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
            return SlLib::Math::Matrix4x4{};
        if (computed[static_cast<std::size_t>(idx)])
            return world[static_cast<std::size_t>(idx)];

        SlLib::Math::Vector4 t{};
        SlLib::Math::Vector4 r{};
        SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
        if (static_cast<std::size_t>(idx) < translations.size())
            t = translations[static_cast<std::size_t>(idx)];
        if (static_cast<std::size_t>(idx) < rotations.size())
            r = rotations[static_cast<std::size_t>(idx)];
        if (static_cast<std::size_t>(idx) < scales.size())
            s = scales[static_cast<std::size_t>(idx)];

        auto local = buildLocalMatrix(t, r, s);
        int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
            world[static_cast<std::size_t>(idx)] = SlLib::Math::Multiply(computeWorld(parentIndex), local);
        else
            world[static_cast<std::size_t>(idx)] = local;

        computed[static_cast<std::size_t>(idx)] = true;
        return world[static_cast<std::size_t>(idx)];
    };

    for (int i = 0; i < static_cast<int>(branchCount); ++i)
        computeWorld(i);

    std::vector<SlLib::Math::Matrix4x4> bindWorld(branchCount);
    std::vector<bool> bindComputed(branchCount, false);
    std::function<SlLib::Math::Matrix4x4(int)> computeBindWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
        if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
            return SlLib::Math::Matrix4x4{};
        if (bindComputed[static_cast<std::size_t>(idx)])
            return bindWorld[static_cast<std::size_t>(idx)];

        SlLib::Math::Vector4 t{};
        SlLib::Math::Vector4 r{};
        SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
        if (static_cast<std::size_t>(idx) < tree->Translations.size())
            t = tree->Translations[static_cast<std::size_t>(idx)];
        if (static_cast<std::size_t>(idx) < tree->Rotations.size())
            r = tree->Rotations[static_cast<std::size_t>(idx)];
        if (static_cast<std::size_t>(idx) < tree->Scales.size())
            s = tree->Scales[static_cast<std::size_t>(idx)];

        auto local = buildLocalMatrix(t, r, s);
        int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
        if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
            bindWorld[static_cast<std::size_t>(idx)] = SlLib::Math::Multiply(computeBindWorld(parentIndex), local);
        else
            bindWorld[static_cast<std::size_t>(idx)] = local;

        bindComputed[static_cast<std::size_t>(idx)] = true;
        return bindWorld[static_cast<std::size_t>(idx)];
    };

    for (int i = 0; i < static_cast<int>(branchCount); ++i)
        computeBindWorld(i);

    _allForestMeshes.clear();
    _allForestMeshes.reserve(_forestMeshSources.size());
    for (auto& source : _forestMeshSources)
    {
        if (source.ForestIndex != forestIndex || source.TreeIndex != treeIndex)
            continue;
        if (source.BranchIndex < 0 || static_cast<std::size_t>(source.BranchIndex) >= branchCount)
            continue;

        Renderer::SlRenderer::ForestCpuMesh cpu;
        cpu.Vertices = source.Vertices;
        cpu.Model = source.Skinned ? IdentityMatrix() : world[static_cast<std::size_t>(source.BranchIndex)];
        cpu.Skinned = source.Skinned;
        if (source.Skinned)
        {
            cpu.BoneMatrixIndices = source.BoneMatrixIndices;
            cpu.BoneInverseMatrices = source.BoneInverseMatrices;

            std::vector<SlLib::Math::Matrix4x4> const* bindWorldPtr = &bindWorld;

            std::size_t n = std::min(source.BoneMatrixIndices.size(), source.BoneInverseMatrices.size());
            cpu.BonePalette.resize(n);
            for (std::size_t bi = 0; bi < n; ++bi)
            {
                int boneIndex = source.BoneMatrixIndices[bi];
                if (boneIndex < 0 || static_cast<std::size_t>(boneIndex) >= world.size())
                {
                    cpu.BonePalette[bi] = IdentityMatrix();
                    continue;
                }
                SlLib::Math::Matrix4x4 inv = source.BoneInverseMatrices[bi];
                if (bindWorldPtr && static_cast<std::size_t>(boneIndex) < bindWorldPtr->size())
                    inv = SlLib::Math::Invert((*bindWorldPtr)[static_cast<std::size_t>(boneIndex)]);
                SlLib::Math::Matrix4x4 w = world[static_cast<std::size_t>(boneIndex)];
                auto pal = SlLib::Math::Multiply(w, inv);
                bool bad = false;
                for (int r = 0; r < 4 && !bad; ++r)
                    for (int c = 0; c < 4; ++c)
                        if (!std::isfinite(pal(r, c)))
                            bad = true;
                cpu.BonePalette[bi] = bad ? IdentityMatrix() : pal;
            }

        }
        cpu.Indices = source.Indices;
        cpu.Texture = source.Texture;
        cpu.ForestIndex = source.ForestIndex;
        cpu.TreeIndex = source.TreeIndex;
        cpu.BranchIndex = source.BranchIndex;
        _allForestMeshes.push_back(std::move(cpu));
    }

    UpdateForestMeshRendering();
}

void CharmyBee::ApplyAnimatorFrame()
{
    if (!_forestLibrary || _forestMeshSources.empty())
        return;

    bool useAnimation = false;
    SeEditor::Forest::SuRenderTree* animTree = nullptr;
    SeEditor::Forest::SuAnimation* animation = nullptr;

    if (_animatorSelectedAnimation >= 0 &&
        _animatorSelectedForest >= 0 &&
        _animatorSelectedTree >= 0 &&
        static_cast<std::size_t>(_animatorSelectedForest) < _forestLibrary->Forests.size())
    {
        auto const& forestEntry = _forestLibrary->Forests[static_cast<std::size_t>(_animatorSelectedForest)];
        if (forestEntry.Forest &&
            static_cast<std::size_t>(_animatorSelectedTree) < forestEntry.Forest->Trees.size())
        {
            animTree = forestEntry.Forest->Trees[static_cast<std::size_t>(_animatorSelectedTree)].get();
            if (animTree && static_cast<std::size_t>(_animatorSelectedAnimation) < animTree->AnimationEntries.size())
            {
                auto const& animEntry = animTree->AnimationEntries[static_cast<std::size_t>(_animatorSelectedAnimation)];
                animation = animEntry.Animation.get();
            }
        }
    }

    if (animation && animTree && animation->Type >= 0x06 && animation->Type <= 0x0A)
    {
        useAnimation = animation->DecodeType6Samples(*animTree);
        if (useAnimation && animation->NumFrames > 0)
        {
            if (_animatorFrame < 0)
                _animatorFrame = 0;
            if (_animatorFrame >= animation->NumFrames)
                _animatorFrame = animation->NumFrames - 1;
        }
    }

    if (std::getenv("ANIM_PRE_DEBUG") != nullptr)
    {
        static auto s_lastAnimPre = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        bool emit = (now - s_lastAnimPre) >= std::chrono::seconds(1);
        if (emit)
            s_lastAnimPre = now;
        int stamp = (_animatorSelectedForest << 16) ^ (_animatorSelectedTree << 8) ^ _animatorSelectedAnimation;
        if (stamp != _animatorDebugStamp)
        {
            _animatorDebugStamp = stamp;
            if (animation)
            {
                std::cout << "[AnimPre] forest=" << _animatorSelectedForest
                          << " tree=" << _animatorSelectedTree
                          << " anim=" << _animatorSelectedAnimation
                          << " type=" << animation->Type
                          << " frames=" << animation->NumFrames
                          << " bones=" << animation->NumBones
                          << " decoded=" << (animation->SamplesDecoded ? 1 : 0)
                          << " paramGpu=" << (animation->Type6ParamDataIsGpu ? 1 : 0)
                          << " blockStart=" << animation->Type6BlockStart
                          << " blockEnd=" << animation->Type6BlockEnd
                          << " paramOffset=" << animation->Type6ParamDataOffset
                          << " dataSize=" << animation->Type6DataSize
                          << std::endl;
            }
            else
            {
                int animCount = 0;
                if (animTree)
                    animCount = static_cast<int>(animTree->AnimationEntries.size());
                std::cout << "[AnimPre] forest=" << _animatorSelectedForest
                          << " tree=" << _animatorSelectedTree
                          << " anim=" << _animatorSelectedAnimation
                          << " animCount=" << animCount
                          << " status=no_animation_selected"
                          << std::endl;
            }
        }
        if (emit)
        {
            bool anyDiff = false;
            bool hasSample = false;
            if (useAnimation && animation && animTree)
            {
                int boneCount = animation->NumBones;
                int checks[] = {0, boneCount / 2, boneCount - 1};
                auto diffVec = [](SlLib::Math::Vector4 const& a, SlLib::Math::Vector4 const& b) {
                    return std::fabs(a.X - b.X) > 1e-4f ||
                           std::fabs(a.Y - b.Y) > 1e-4f ||
                           std::fabs(a.Z - b.Z) > 1e-4f ||
                           std::fabs(a.W - b.W) > 1e-4f;
                };
                for (int idx : checks)
                {
                    if (idx < 0 || idx >= boneCount)
                        continue;
                    auto sample = animation->GetSample(_animatorFrame, idx);
                    if (!sample)
                        continue;
                    hasSample = true;
                    SlLib::Math::Vector4 bt{};
                    SlLib::Math::Vector4 br{};
                    SlLib::Math::Vector4 bs{1.0f, 1.0f, 1.0f, 1.0f};
                    if (static_cast<std::size_t>(idx) < animTree->Translations.size())
                        bt = animTree->Translations[static_cast<std::size_t>(idx)];
                    if (static_cast<std::size_t>(idx) < animTree->Rotations.size())
                        br = animTree->Rotations[static_cast<std::size_t>(idx)];
                    if (static_cast<std::size_t>(idx) < animTree->Scales.size())
                        bs = animTree->Scales[static_cast<std::size_t>(idx)];
                    if (diffVec(sample->Translation, bt) ||
                        diffVec(sample->Rotation, br) ||
                        diffVec(sample->Scale, bs))
                    {
                        anyDiff = true;
                        break;
                    }
                }
            }
            std::cout << "[AnimPre] frame=" << _animatorFrame
                      << " useAnimation=" << (useAnimation ? 1 : 0)
                      << " treeMatch=" << ((animTree != nullptr) ? 1 : 0)
                      << " hasSample=" << (hasSample ? 1 : 0)
                      << " anyDiff=" << (anyDiff ? 1 : 0)
                      << " maskNonZero=" << (animation ? animation->Type6MaskNonZero : 0)
                      << " mask0=0x" << std::hex << (animation ? animation->Type6MaskSample[0] : 0u)
                      << " mask1=0x" << (animation ? animation->Type6MaskSample[1] : 0u)
                      << " mask2=0x" << (animation ? animation->Type6MaskSample[2] : 0u)
                      << std::dec
                      << std::endl;
        }
    }

    std::vector<std::vector<std::vector<SlLib::Math::Matrix4x4>>> worldByForest;
    std::vector<std::vector<std::vector<SlLib::Math::Matrix4x4>>> bindWorldByForest;
    worldByForest.resize(_forestLibrary->Forests.size());
    bindWorldByForest.resize(_forestLibrary->Forests.size());
    std::vector<bool> animVisibility;
    int animForestIndex = _animatorSelectedForest;
    int animTreeIndex = _animatorSelectedTree;
    bool hasAnimVisibility = false;

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

    for (std::size_t forestIdx = 0; forestIdx < _forestLibrary->Forests.size(); ++forestIdx)
    {
        auto const& forestEntry = _forestLibrary->Forests[forestIdx];
        if (!forestEntry.Forest)
            continue;
        auto const& trees = forestEntry.Forest->Trees;
        worldByForest[forestIdx].resize(trees.size());
        bindWorldByForest[forestIdx].resize(trees.size());

        for (std::size_t treeIdx = 0; treeIdx < trees.size(); ++treeIdx)
        {
            auto const& tree = trees[treeIdx];
            if (!tree)
                continue;

            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);

            std::vector<SlLib::Math::Vector4> t = tree->Translations;
            std::vector<SlLib::Math::Vector4> r = tree->Rotations;
            std::vector<SlLib::Math::Vector4> s = tree->Scales;

            std::vector<SlLib::Math::Matrix4x4> bindWorld(branchCount);
            std::vector<bool> bindComputed(branchCount, false);
            std::function<SlLib::Math::Matrix4x4(int)> computeBindWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
                if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                    return SlLib::Math::Matrix4x4{};
                if (bindComputed[static_cast<std::size_t>(idx)])
                    return bindWorld[static_cast<std::size_t>(idx)];

                SlLib::Math::Vector4 lt{};
                SlLib::Math::Vector4 lr{};
                SlLib::Math::Vector4 ls{1.0f, 1.0f, 1.0f, 1.0f};
                if (static_cast<std::size_t>(idx) < tree->Translations.size())
                    lt = tree->Translations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Rotations.size())
                    lr = tree->Rotations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Scales.size())
                    ls = tree->Scales[static_cast<std::size_t>(idx)];

                auto local = buildLocalMatrix(lt, lr, ls);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                    bindWorld[static_cast<std::size_t>(idx)] = SlLib::Math::Multiply(computeBindWorld(parentIndex), local);
                else
                    bindWorld[static_cast<std::size_t>(idx)] = local;

                bindComputed[static_cast<std::size_t>(idx)] = true;
                return bindWorld[static_cast<std::size_t>(idx)];
            };

            for (int i = 0; i < static_cast<int>(branchCount); ++i)
                computeBindWorld(i);

            if (useAnimation &&
                animTree == tree.get() &&
                forestIdx == static_cast<std::size_t>(_animatorSelectedForest) &&
                treeIdx == static_cast<std::size_t>(_animatorSelectedTree))
            {
                t.resize(branchCount);
                r.resize(branchCount);
                s.resize(branchCount);
                animVisibility.assign(branchCount, true);
                hasAnimVisibility = true;
                for (std::size_t bone = 0; bone < branchCount; ++bone)
                {
                    auto sample = animation->GetSample(_animatorFrame, static_cast<int>(bone));
                    if (!sample)
                        continue;
                    t[bone] = sample->Translation;
                    r[bone] = sample->Rotation;
                    s[bone] = sample->Scale;
                    animVisibility[bone] = sample->Visible;
                }
            }

            std::function<SlLib::Math::Matrix4x4(int)> computeWorld = [&](int idx) -> SlLib::Math::Matrix4x4 {
                if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                    return SlLib::Math::Matrix4x4{};
                if (computed[static_cast<std::size_t>(idx)])
                    return world[static_cast<std::size_t>(idx)];

                SlLib::Math::Vector4 lt{};
                SlLib::Math::Vector4 lr{};
                SlLib::Math::Vector4 ls{1.0f, 1.0f, 1.0f, 1.0f};
                if (static_cast<std::size_t>(idx) < t.size())
                    lt = t[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < r.size())
                    lr = r[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < s.size())
                    ls = s[static_cast<std::size_t>(idx)];

                auto local = buildLocalMatrix(lt, lr, ls);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                    world[static_cast<std::size_t>(idx)] = SlLib::Math::Multiply(computeWorld(parentIndex), local);
                else
                    world[static_cast<std::size_t>(idx)] = local;

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            for (int i = 0; i < static_cast<int>(branchCount); ++i)
                computeWorld(i);

            worldByForest[forestIdx][treeIdx] = std::move(world);
            bindWorldByForest[forestIdx][treeIdx] = std::move(bindWorld);

            if (std::getenv("RENDER_PRE_DEBUG") != nullptr &&
                useAnimation &&
                animTree == tree.get() &&
                forestIdx == static_cast<std::size_t>(_animatorSelectedForest) &&
                treeIdx == static_cast<std::size_t>(_animatorSelectedTree))
            {
                static auto s_last = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (now - s_last >= std::chrono::seconds(1))
                {
                    s_last = now;
                    if (!worldByForest[forestIdx][treeIdx].empty())
                    {
                        auto const& wm = worldByForest[forestIdx][treeIdx].front();
                        std::cout << "[RenderPre] world0 m00=" << wm(0, 0)
                                  << " m03=" << wm(0, 3)
                                  << " m13=" << wm(1, 3)
                                  << " m23=" << wm(2, 3)
                                  << std::endl;
                    }
                }
            }
        }
    }

    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> animatorBoneLines;
    if (_drawAnimatorBones && hasAnimVisibility &&
        animForestIndex >= 0 && animTreeIndex >= 0 &&
        animForestIndex < static_cast<int>(worldByForest.size()))
    {
        auto const& treeWorlds = worldByForest[static_cast<std::size_t>(animForestIndex)];
        if (animTreeIndex < static_cast<int>(treeWorlds.size()))
        {
            auto const& world = treeWorlds[static_cast<std::size_t>(animTreeIndex)];
            auto const& branches = animTree ? animTree->Branches : std::vector<std::shared_ptr<Forest::SuBranch>>{};
            auto extractTranslation = [](SlLib::Math::Matrix4x4 const& matrix) {
                return SlLib::Math::Vector3{matrix(0, 3), matrix(1, 3), matrix(2, 3)};
            };
            for (int branchIdx = 0; branchIdx < static_cast<int>(branches.size()); ++branchIdx)
            {
                auto const& branch = branches[static_cast<std::size_t>(branchIdx)];
                if (!branch)
                    continue;
                int parent = branch->Parent;
                if (parent < 0)
                    continue;
                if (!IsBranchVisible(animForestIndex, animTreeIndex, branchIdx))
                    continue;
                if (!IsBranchVisible(animForestIndex, animTreeIndex, parent))
                    continue;
                if (static_cast<std::size_t>(branchIdx) >= world.size() ||
                    static_cast<std::size_t>(parent) >= world.size())
                {
                    continue;
                }
                animatorBoneLines.emplace_back(extractTranslation(world[static_cast<std::size_t>(parent)]),
                                               extractTranslation(world[static_cast<std::size_t>(branchIdx)]));
            }
        }
    }
    _renderer.SetBoneLines(animatorBoneLines);
    _renderer.SetDrawBoneLines(_drawAnimatorBones && !animatorBoneLines.empty());

    std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
    meshes.reserve(_forestMeshSources.size());
    if (hasAnimVisibility)
    {
        _animatorBranchVisibility = std::move(animVisibility);
        _animatorVisibilityForest = animForestIndex;
        _animatorVisibilityTree = animTreeIndex;
    }
    else
    {
        _animatorBranchVisibility.clear();
        _animatorVisibilityForest = -1;
        _animatorVisibilityTree = -1;
    }

    for (auto& source : _forestMeshSources)
    {
        if (source.ForestIndex < 0 || source.TreeIndex < 0 || source.BranchIndex < 0)
            continue;
        if (static_cast<std::size_t>(source.ForestIndex) >= worldByForest.size())
            continue;
        auto const& treeWorlds = worldByForest[static_cast<std::size_t>(source.ForestIndex)];
        if (static_cast<std::size_t>(source.TreeIndex) >= treeWorlds.size())
            continue;
        auto const& world = treeWorlds[static_cast<std::size_t>(source.TreeIndex)];
        if (static_cast<std::size_t>(source.BranchIndex) >= world.size())
            continue;

        Renderer::SlRenderer::ForestCpuMesh cpu;
        cpu.Vertices = source.Vertices;
        cpu.Model = source.Skinned ? IdentityMatrix() : world[static_cast<std::size_t>(source.BranchIndex)];
        cpu.Skinned = source.Skinned;
        if (source.Skinned)
        {
            cpu.BoneMatrixIndices = source.BoneMatrixIndices;
            cpu.BoneInverseMatrices = source.BoneInverseMatrices;

            std::vector<SlLib::Math::Matrix4x4> const* bindWorldPtr = nullptr;
            if (static_cast<std::size_t>(source.ForestIndex) < bindWorldByForest.size())
            {
                auto const& bindTrees = bindWorldByForest[static_cast<std::size_t>(source.ForestIndex)];
                if (static_cast<std::size_t>(source.TreeIndex) < bindTrees.size())
                    bindWorldPtr = &bindTrees[static_cast<std::size_t>(source.TreeIndex)];
            }

            std::size_t n = std::min(source.BoneMatrixIndices.size(), source.BoneInverseMatrices.size());
            cpu.BonePalette.resize(n);
            for (std::size_t bi = 0; bi < n; ++bi)
            {
                int boneIndex = source.BoneMatrixIndices[bi];
                if (boneIndex < 0 || static_cast<std::size_t>(boneIndex) >= world.size())
                {
                    cpu.BonePalette[bi] = IdentityMatrix();
                    continue;
                }
                SlLib::Math::Matrix4x4 inv = source.BoneInverseMatrices[bi];
                if (bindWorldPtr && static_cast<std::size_t>(boneIndex) < bindWorldPtr->size())
                    inv = SlLib::Math::Invert((*bindWorldPtr)[static_cast<std::size_t>(boneIndex)]);
                SlLib::Math::Matrix4x4 w = world[static_cast<std::size_t>(boneIndex)];
                auto pal = SlLib::Math::Multiply(w, inv);
                bool bad = false;
                for (int r = 0; r < 4 && !bad; ++r)
                    for (int c = 0; c < 4; ++c)
                        if (!std::isfinite(pal(r, c)))
                            bad = true;
                cpu.BonePalette[bi] = bad ? IdentityMatrix() : pal;
            }

            if (std::getenv("BONE_DEBUG") != nullptr &&
                useAnimation && animation && animTree &&
                source.ForestIndex == _animatorSelectedForest &&
                source.TreeIndex == _animatorSelectedTree)
            {
                static auto s_last = std::chrono::steady_clock::now();
                auto now = std::chrono::steady_clock::now();
                if (now - s_last >= std::chrono::seconds(1))
                {
                    s_last = now;
                    auto emitBone = [&](int boneIdx) {
                        auto sample = animation->GetSample(_animatorFrame, boneIdx);
                        float qLen = 0.0f;
                        if (sample)
                        {
                            qLen = std::sqrt(sample->Rotation.X * sample->Rotation.X +
                                             sample->Rotation.Y * sample->Rotation.Y +
                                             sample->Rotation.Z * sample->Rotation.Z +
                                             sample->Rotation.W * sample->Rotation.W);
                        }
                        int parentIdx = -1;
                        if (animTree && static_cast<std::size_t>(boneIdx) < animTree->Branches.size() &&
                            animTree->Branches[static_cast<std::size_t>(boneIdx)])
                        {
                            parentIdx = animTree->Branches[static_cast<std::size_t>(boneIdx)]->Parent;
                        }
                        int paletteIndex = -1;
                        for (std::size_t bi = 0; bi < n; ++bi)
                        {
                            if (source.BoneMatrixIndices[bi] == boneIdx)
                            {
                                paletteIndex = static_cast<int>(bi);
                                break;
                            }
                        }
                        std::cout << "[BoneDbg] bone=" << boneIdx
                                  << " parent=" << parentIdx
                                  << " sample=" << (sample ? 1 : 0)
                                  << " qLen=" << qLen
                                  << " palette=" << paletteIndex;
                        if (sample)
                        {
                            std::cout << " T=(" << sample->Translation.X << "," << sample->Translation.Y << "," << sample->Translation.Z << ")"
                                      << " R=(" << sample->Rotation.X << "," << sample->Rotation.Y << "," << sample->Rotation.Z << "," << sample->Rotation.W << ")"
                                      << " S=(" << sample->Scale.X << "," << sample->Scale.Y << "," << sample->Scale.Z << ")";
                        }
                        if (animTree && static_cast<std::size_t>(boneIdx) < animTree->Translations.size())
                        {
                            auto const& bt = animTree->Translations[static_cast<std::size_t>(boneIdx)];
                            auto const& br = animTree->Rotations[static_cast<std::size_t>(boneIdx)];
                            auto const& bs = animTree->Scales[static_cast<std::size_t>(boneIdx)];
                            std::cout << " bindT=(" << bt.X << "," << bt.Y << "," << bt.Z << ")"
                                      << " bindR=(" << br.X << "," << br.Y << "," << br.Z << "," << br.W << ")"
                                      << " bindS=(" << bs.X << "," << bs.Y << "," << bs.Z << ")";
                        }
                        if (static_cast<std::size_t>(boneIdx) < world.size())
                        {
                            auto const& wm = world[static_cast<std::size_t>(boneIdx)];
                            std::cout << " Wm00=" << wm(0, 0) << " Wm03=" << wm(0, 3);
                        }
                        if (parentIdx >= 0 && static_cast<std::size_t>(parentIdx) < world.size())
                        {
                            auto const& pwm = world[static_cast<std::size_t>(parentIdx)];
                            std::cout << " PWm03=" << pwm(0, 3) << " PWm13=" << pwm(1, 3) << " PWm23=" << pwm(2, 3);
                        }
                        if (paletteIndex >= 0)
                        {
                            auto const& invSource = source.BoneInverseMatrices[static_cast<std::size_t>(paletteIndex)];
                            auto const& pal = cpu.BonePalette[static_cast<std::size_t>(paletteIndex)];
                            SlLib::Math::Matrix4x4 invUsed = invSource;
                            auto const& bindTreeWorlds = bindWorldByForest[static_cast<std::size_t>(source.ForestIndex)];
                            if (static_cast<std::size_t>(source.TreeIndex) < bindTreeWorlds.size() &&
                                static_cast<std::size_t>(boneIdx) < bindTreeWorlds[static_cast<std::size_t>(source.TreeIndex)].size())
                            {
                                invUsed = SlLib::Math::Invert(bindTreeWorlds[static_cast<std::size_t>(source.TreeIndex)][static_cast<std::size_t>(boneIdx)]);
                            }
                            std::cout << " inv00=" << invSource(0, 0) << " inv03=" << invSource(0, 3)
                                      << " invUsed00=" << invUsed(0, 0) << " invUsed03=" << invUsed(0, 3)
                                      << " pal00=" << pal(0, 0) << " pal03=" << pal(0, 3);
                        }
                        auto const& bindTreeWorlds = bindWorldByForest[static_cast<std::size_t>(source.ForestIndex)];
                        if (static_cast<std::size_t>(source.TreeIndex) < bindTreeWorlds.size() &&
                            static_cast<std::size_t>(boneIdx) < bindTreeWorlds[static_cast<std::size_t>(source.TreeIndex)].size())
                        {
                            auto const& bw = bindTreeWorlds[static_cast<std::size_t>(source.TreeIndex)][static_cast<std::size_t>(boneIdx)];
                            std::cout << " BWm03=" << bw(0, 3) << " BWm13=" << bw(1, 3) << " BWm23=" << bw(2, 3);
                        }
                        std::cout << std::endl;
                    };
                    emitBone(15);
                    emitBone(16);
                }
            }
        }
        cpu.Indices = source.Indices;
        cpu.Texture = source.Texture;
        cpu.ForestIndex = source.ForestIndex;
        cpu.TreeIndex = source.TreeIndex;
        cpu.BranchIndex = source.BranchIndex;
        meshes.push_back(std::move(cpu));
    }

    if (std::getenv("RENDER_PRE_DEBUG") != nullptr)
    {
        static auto s_last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - s_last >= std::chrono::seconds(1))
        {
            s_last = now;
            for (auto const& mesh : meshes)
            {
                if (!mesh.Skinned || mesh.BonePalette.empty())
                    continue;
                auto const& m = mesh.BonePalette.front();
                std::cout << "[RenderPre] producer firstBone m00=" << m(0, 0)
                          << " m03=" << m(0, 3)
                          << " m13=" << m(1, 3)
                          << " m23=" << m(2, 3)
                          << std::endl;
                break;
            }
        }
    }

    _allForestMeshes = std::move(meshes);
    UpdateForestMeshRendering();
    _animatorLastAppliedFrame = _animatorFrame;
}

void CharmyBee::UpdateAnimator(float deltaSeconds)
{
    if (!_forestLibrary || _forestMeshSources.empty())
        return;

    SeEditor::Forest::SuAnimation* animation = nullptr;
    SeEditor::Forest::SuRenderTree* tree = nullptr;
    static float s_animDebugTimer = 0.0f;

    if (_animatorSelectedAnimation >= 0 &&
        _animatorSelectedForest >= 0 &&
        _animatorSelectedTree >= 0 &&
        static_cast<std::size_t>(_animatorSelectedForest) < _forestLibrary->Forests.size())
    {
        auto const& forestEntry = _forestLibrary->Forests[static_cast<std::size_t>(_animatorSelectedForest)];
        if (forestEntry.Forest &&
            static_cast<std::size_t>(_animatorSelectedTree) < forestEntry.Forest->Trees.size())
        {
            tree = forestEntry.Forest->Trees[static_cast<std::size_t>(_animatorSelectedTree)].get();
            if (tree && static_cast<std::size_t>(_animatorSelectedAnimation) < tree->AnimationEntries.size())
                animation = tree->AnimationEntries[static_cast<std::size_t>(_animatorSelectedAnimation)].Animation.get();
        }
    }

    if (_animatorPlaying && animation && animation->NumFrames > 0)
    {
        if (_animatorFps <= 0.0f)
            _animatorFps = 30.0f;
        _animatorTime += deltaSeconds;
        _animatorFrameAccumulator += deltaSeconds * _animatorFps;
        int maxFrames = animation->NumFrames;
        if (maxFrames > 0)
        {
            bool advanced = false;
            while (_animatorFrameAccumulator >= 1.0f)
            {
                _animatorFrameAccumulator -= 1.0f;
                _animatorFrame = (_animatorFrame + 1) % maxFrames;
                advanced = true;
            }
            if (advanced)
                _animatorDirty = true;
        }
        else
        {
            _animatorDirty = true;
        }
    }

    if (!animation && _animatorLastAppliedFrame >= 0)
        _animatorDirty = true;

    (void)s_animDebugTimer;

    if (_animatorDirty)
    {
        ApplyAnimatorFrame();
        _animatorDirty = false;
    }
}

bool CharmyBee::RenderForestBoxLayer(ForestBoxLayer& layer)
{
    std::string label = layer.Name.empty() ? "Unnamed" : layer.Name;
    label += "###forest_" + std::to_string(reinterpret_cast<std::uintptr_t>(&layer));
    bool changed = ImGui::Checkbox(label.c_str(), &layer.Visible);
    if (changed)
    {
        for (auto& child : layer.Children)
            SetForestLayerVisibilityRecursive(child, layer.Visible);
        SaveForestVisibility();
    }
    if (!layer.Children.empty())
    {
        ImGui::Indent();
        for (auto& child : layer.Children)
            changed |= RenderForestBoxLayer(child);
        ImGui::Unindent();
    }
    return changed;
}

void CharmyBee::SetForestLayerVisibilityRecursive(ForestBoxLayer& layer, bool visible)
{
    layer.Visible = visible;
    for (auto& child : layer.Children)
        SetForestLayerVisibilityRecursive(child, visible);
}

std::filesystem::path CharmyBee::GetForestVisibilityPath() const
{
    std::filesystem::path base;
    if (!_sifFilePath.empty())
    {
        base = std::filesystem::path(_sifFilePath).filename();
        base.replace_extension("");
    }
    else
    {
        base = "forest";
    }

    std::string filename = base.string() + "_forestXYZ.txt";
    return std::filesystem::current_path() / filename;
}

std::filesystem::path CharmyBee::GetForestHierarchyVisibilityPath() const
{
    std::filesystem::path base;
    std::filesystem::path root = std::filesystem::current_path();
    if (!_sifFilePath.empty())
    {
        std::filesystem::path sifPath(_sifFilePath);
        base = sifPath.filename();
        if (!sifPath.parent_path().empty())
            root = sifPath.parent_path();
        base.replace_extension("");
    }
    else
    {
        base = "forest";
    }

    std::string filename = base.string() + "_forestHierarchy.txt";
    return root / filename;
}

std::filesystem::path CharmyBee::GetAnimatorSettingsPath() const
{
    std::filesystem::path base;
    std::filesystem::path root = std::filesystem::current_path();
    if (!_sifFilePath.empty())
    {
        std::filesystem::path sifPath(_sifFilePath);
        base = sifPath.filename();
        if (!sifPath.parent_path().empty())
            root = sifPath.parent_path();
        base.replace_extension("");
    }
    else
    {
        base = "forest";
    }

    std::string filename = base.string() + "_animator.txt";
    return root / filename;
}

void CharmyBee::SaveAnimatorSettings() const
{
    std::filesystem::path outPath = GetAnimatorSettingsPath();
    std::ofstream out(outPath, std::ios::binary);
    if (!out)
        return;

    out << "forest=" << _animatorSelectedForest << "\n";
    out << "tree=" << _animatorSelectedTree << "\n";
    out << "animation=" << _animatorSelectedAnimation << "\n";
    out << "branch=" << _animatorSelectedBranch << "\n";
    out << "frame=" << _animatorFrame << "\n";
    out << "time=" << _animatorTime << "\n";
    out << "fps=" << _animatorFps << "\n";
    out << "playing=" << (_animatorPlaying ? 1 : 0) << "\n";
    out << "renderBones=" << (_drawAnimatorBones ? 1 : 0) << "\n";
}

void CharmyBee::LoadAnimatorSettings()
{
    std::filesystem::path inPath = GetAnimatorSettingsPath();
    if (std::getenv("ANIM_PRE_DEBUG") != nullptr)
    {
        std::error_code ec;
        bool exists = std::filesystem::exists(inPath, ec);
        std::cout << "[AnimPre] settingsPath=" << inPath.string()
                  << " exists=" << (exists ? 1 : 0)
                  << std::endl;
    }
    std::ifstream in(inPath, std::ios::binary);
    if (!in && !_sifFilePath.empty())
    {
        std::filesystem::path legacy = std::filesystem::current_path() / inPath.filename();
        if (legacy != inPath)
            in = std::ifstream(legacy, std::ios::binary);
    }
    if (!in)
        return;

    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty())
            continue;
        std::size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);
        try
        {
            if (key == "forest")
                _animatorSelectedForest = std::stoi(value);
            else if (key == "tree")
                _animatorSelectedTree = std::stoi(value);
            else if (key == "animation")
                _animatorSelectedAnimation = std::stoi(value);
            else if (key == "branch")
                _animatorSelectedBranch = std::stoi(value);
            else if (key == "frame")
                _animatorFrame = std::stoi(value);
            else if (key == "time")
                _animatorTime = std::stof(value);
            else if (key == "fps")
                _animatorFps = std::stof(value);
            else if (key == "playing")
                _animatorPlaying = (std::stoi(value) != 0);
            else if (key == "renderBones")
                _drawAnimatorBones = (std::stoi(value) != 0);
        }
        catch (...)
        {
        }
    }

    if (_animatorFps <= 0.0f)
        _animatorFps = 30.0f;

    if (_forestLibrary && !_forestLibrary->Forests.empty())
    {
        if (_animatorSelectedForest < 0)
            _animatorSelectedForest = 0;
        if (static_cast<std::size_t>(_animatorSelectedForest) >= _forestLibrary->Forests.size())
            _animatorSelectedForest = static_cast<int>(_forestLibrary->Forests.size()) - 1;

        auto const& forestEntry = _forestLibrary->Forests[static_cast<std::size_t>(_animatorSelectedForest)];
        if (forestEntry.Forest && !forestEntry.Forest->Trees.empty())
        {
            if (_animatorSelectedTree < 0)
                _animatorSelectedTree = 0;
            if (static_cast<std::size_t>(_animatorSelectedTree) >= forestEntry.Forest->Trees.size())
                _animatorSelectedTree = static_cast<int>(forestEntry.Forest->Trees.size()) - 1;

            auto const& tree = forestEntry.Forest->Trees[static_cast<std::size_t>(_animatorSelectedTree)];
            if (tree)
            {
                int animCount = static_cast<int>(tree->AnimationEntries.size());
                if (_animatorSelectedAnimation >= animCount)
                    _animatorSelectedAnimation = animCount > 0 ? 0 : -1;
                if (_animatorSelectedAnimation < -1)
                    _animatorSelectedAnimation = -1;

                int branchCount = static_cast<int>(tree->Branches.size());
                if (_animatorSelectedBranch < 0)
                    _animatorSelectedBranch = 0;
                if (_animatorSelectedBranch >= branchCount)
                    _animatorSelectedBranch = branchCount > 0 ? branchCount - 1 : 0;
            }
        }
    }

    _animatorFrameAccumulator = 0.0f;
    _animatorDirty = true;

    if (std::getenv("ANIM_PRE_DEBUG") != nullptr)
    {
        std::cout << "[AnimPre] settingsLoaded forest=" << _animatorSelectedForest
                  << " tree=" << _animatorSelectedTree
                  << " anim=" << _animatorSelectedAnimation
                  << " branch=" << _animatorSelectedBranch
                  << " fps=" << _animatorFps
                  << " playing=" << (_animatorPlaying ? 1 : 0)
                  << std::endl;
    }
}

void CharmyBee::SaveForestVisibility() const
{
    std::unordered_set<std::string> visible;
    std::function<void(ForestBoxLayer const&, std::string const&)> gather =
        [&](ForestBoxLayer const& layer, std::string const& prefix) {
            std::string name = layer.Name.empty() ? "Unnamed" : layer.Name;
            std::string path = prefix.empty() ? name : prefix + "/" + name;
            if (layer.Visible)
                visible.insert(path);
            for (auto const& child : layer.Children)
                gather(child, path);
        };

    for (auto const& layer : _forestBoxLayers)
        gather(layer, "");

    std::filesystem::path outPath = GetForestVisibilityPath();
    std::ofstream out(outPath, std::ios::binary);
    if (!out)
        return;

    for (auto const& entry : visible)
        out << entry << "\n";
}

void CharmyBee::SaveForestHierarchyVisibility() const
{
    std::filesystem::path outPath = GetForestHierarchyVisibilityPath();
    std::ofstream out(outPath, std::ios::binary);
    if (!out)
        return;

    for (std::size_t fi = 0; fi < _forestHierarchy.size(); ++fi)
    {
        auto const& forest = _forestHierarchy[fi];
        out << "F " << fi << " " << (forest.Visible ? 1 : 0) << "\n";
        for (std::size_t ti = 0; ti < forest.Trees.size(); ++ti)
        {
            auto const& tree = forest.Trees[ti];
            out << "F " << fi << " T " << ti << " " << (tree.Visible ? 1 : 0) << "\n";
            for (std::size_t bi = 0; bi < tree.Nodes.size(); ++bi)
            {
                auto const& node = tree.Nodes[bi];
                out << "F " << fi << " T " << ti << " B " << bi << " " << (node.Visible ? 1 : 0) << "\n";
            }
        }
    }
}

void CharmyBee::LoadForestVisibility()
{
    std::filesystem::path inPath = GetForestVisibilityPath();
    std::ifstream in(inPath, std::ios::binary);
    if (!in)
        return;

    std::unordered_set<std::string> visible;
    std::string line;
    while (std::getline(in, line))
    {
        if (!line.empty())
            visible.insert(line);
    }

        if (visible.empty())
    {
        printf("[CharmyBee] Visibility file exists but is empty -> ignoring\n");
        return;
    }

    std::function<bool(ForestBoxLayer&, std::string const&)> apply =
        [&](ForestBoxLayer& layer, std::string const& prefix) -> bool {
            std::string name = layer.Name.empty() ? "Unnamed" : layer.Name;
            std::string path = prefix.empty() ? name : prefix + "/" + name;
            bool isVisible = visible.find(path) != visible.end();
            bool anyChildVisible = false;
            for (auto& child : layer.Children)
                anyChildVisible |= apply(child, path);
            layer.Visible = isVisible || anyChildVisible;
            return layer.Visible;
        };

    for (auto& layer : _forestBoxLayers)
        apply(layer, "");
}

void CharmyBee::LoadForestHierarchyVisibility()
{
    std::filesystem::path inPath = GetForestHierarchyVisibilityPath();
    std::ifstream in(inPath, std::ios::binary);
    if (!in && !_sifFilePath.empty())
    {
        std::filesystem::path legacy = std::filesystem::current_path() / inPath.filename();
        if (legacy != inPath)
            in = std::ifstream(legacy, std::ios::binary);
    }
    if (!in)
        return;

    std::string token;
    while (in >> token)
    {
        if (token != "F")
        {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::size_t fi = 0;
        if (!(in >> fi))
            break;
        if (fi >= _forestHierarchy.size())
        {
            in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::string next;
        if (!(in >> next))
            break;
        if (next == "T")
        {
            std::size_t ti = 0;
            if (!(in >> ti))
                break;
            if (ti >= _forestHierarchy[fi].Trees.size())
            {
                in.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                continue;
            }
            std::string maybeB;
            if (!(in >> maybeB))
                break;
            if (maybeB == "B")
            {
                std::size_t bi = 0;
                int vis = 1;
                if (!(in >> bi >> vis))
                    break;
                if (bi < _forestHierarchy[fi].Trees[ti].Nodes.size())
                    _forestHierarchy[fi].Trees[ti].Nodes[bi].Visible = (vis != 0);
            }
            else
            {
                int vis = std::stoi(maybeB);
                _forestHierarchy[fi].Trees[ti].Visible = (vis != 0);
            }
        }
        else
        {
            int vis = std::stoi(next);
            _forestHierarchy[fi].Visible = (vis != 0);
        }
    }
}

void CharmyBee::ExportForestObj(std::filesystem::path const& outputPath,
                                std::string const& forestNameFilter)
{
    if (!_forestLibrary)
        LoadForestResources();

    if (!_forestLibrary || _forestLibrary->Forests.empty())
    {
        std::cerr << "[CharmyBee] No forest data loaded to export." << std::endl;
        return;
    }

    std::filesystem::path outDir = outputPath.parent_path();
    if (outDir.empty())
        outDir = std::filesystem::current_path();

    std::filesystem::path mtlPath = outputPath;
    mtlPath.replace_extension(".mtl");

    std::ofstream obj(outputPath, std::ios::binary);
    if (!obj)
    {
        std::cerr << "[CharmyBee] Failed to open OBJ file for writing: " << outputPath.string() << std::endl;
        return;
    }

    std::ofstream mtl(mtlPath, std::ios::binary);
    if (!mtl)
    {
        std::cerr << "[CharmyBee] Failed to open MTL file for writing: " << mtlPath.string() << std::endl;
        return;
    }

    auto sanitize = [](std::string name) {
        for (char& c : name)
        {
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'))
                c = '_';
        }
        if (name.empty())
            name = "unnamed";
        return name;
    };

    struct ForestVertex
    {
        SlLib::Math::Vector3 Pos{};
        SlLib::Math::Vector3 Normal{0.0f, 1.0f, 0.0f};
        SlLib::Math::Vector2 Uv{};
    };

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

    auto decodeVertex = [&](SeEditor::Forest::SuRenderVertexStream const& stream) {
        std::vector<ForestVertex> verts;
        if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
            return verts;

        verts.resize(static_cast<std::size_t>(stream.VertexCount));
        for (int i = 0; i < stream.VertexCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
            ForestVertex v;
            for (auto const& attr : stream.AttributeStreamsInfo)
            {
                if (attr.Stream != 0)
                    continue;

                std::size_t off = base + static_cast<std::size_t>(attr.Offset);
                using SeEditor::Forest::D3DDeclType;
                using SeEditor::Forest::D3DDeclUsage;

                if (attr.Usage == D3DDeclUsage::Position)
                {
                    std::size_t posOff = off;
                    if (stream.StreamBias != 0)
                        posOff += static_cast<std::size_t>(stream.StreamBias);
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float4)
                    {
                        v.Pos = {readFloat(stream.Stream, posOff + 0),
                                 readFloat(stream.Stream, posOff + 4),
                                 readFloat(stream.Stream, posOff + 8)};
                    }
                }
                else if (attr.Usage == D3DDeclUsage::Normal)
                {
                    if (attr.Type == D3DDeclType::Float3)
                    {
                        v.Normal = {readFloat(stream.Stream, off + 0),
                                    readFloat(stream.Stream, off + 4),
                                    readFloat(stream.Stream, off + 8)};
                    }
                    else if (attr.Type == D3DDeclType::Float16x4)
                    {
                        v.Normal = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                    HalfToFloat(readU16(stream.Stream, off + 2)),
                                    HalfToFloat(readU16(stream.Stream, off + 4))};
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
                    else if (attr.Type == D3DDeclType::Float16x2)
                    {
                        v.Uv = {HalfToFloat(readU16(stream.Stream, off + 0)),
                                HalfToFloat(readU16(stream.Stream, off + 2))};
                    }
                }
            }

            verts[static_cast<std::size_t>(i)] = v;
        }

        return verts;
    };

    auto buildLocalMatrix = [](SlLib::Math::Vector4 t, SlLib::Math::Vector4 r, SlLib::Math::Vector4 s) {
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

    std::unordered_map<SeEditor::Forest::SuRenderTextureResource*, std::string> materialNames;
    std::unordered_map<SeEditor::Forest::SuRenderTextureResource*, std::string> textureFiles;
    int materialCounter = 0;

    auto getMaterialName = [&](std::shared_ptr<SeEditor::Forest::SuRenderTextureResource> const& tex) {
        if (!tex)
            return std::string("default");
        auto* key = tex.get();
        auto it = materialNames.find(key);
        if (it != materialNames.end())
            return it->second;

        std::string base = tex->Name.empty() ? ("tex_" + std::to_string(materialCounter++))
                                             : sanitize(std::filesystem::path(tex->Name).filename().string());
        std::string mtlName = base;
        materialNames[key] = mtlName;

        if (!tex->ImageData.empty())
        {
            std::string texFile = base;
            if (std::filesystem::path(texFile).extension().empty())
                texFile += ".dds";
            textureFiles[key] = texFile;
            std::filesystem::path outTex = outDir / texFile;
            std::ofstream texOut(outTex, std::ios::binary);
            if (texOut)
                texOut.write(reinterpret_cast<char const*>(tex->ImageData.data()),
                             static_cast<std::streamsize>(tex->ImageData.size()));
        }

        return mtlName;
    };

    obj << "# Exported from CppSLib Forest\n";
    obj << "mtllib " << mtlPath.filename().string() << "\n";

    std::size_t globalVertexBase = 1;
    int objectCounter = 0;

    for (auto const& forestEntry : _forestLibrary->Forests)
    {
        if (!forestEntry.Forest)
            continue;
        if (!forestNameFilter.empty() && forestEntry.Name != forestNameFilter)
            continue;

        for (auto const& tree : forestEntry.Forest->Trees)
        {
            if (!tree)
                continue;

            std::size_t branchCount = tree->Branches.size();
            std::vector<SlLib::Math::Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);

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

                auto local = buildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        SlLib::Math::Multiply(computeWorld(parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                                  SlLib::Math::Matrix4x4 const& worldMatrix) {
                if (!mesh)
                    return;

                for (auto const& primitive : mesh->Primitives)
                {
                    if (!primitive || !primitive->VertexStream)
                        continue;

                    auto verts = decodeVertex(*primitive->VertexStream);
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

                    std::string objName = sanitize(forestEntry.Name) + "_" + std::to_string(objectCounter++);
                    obj << "o " << objName << "\n";

                    std::string material = "default";
                    if (primitive->Material && !primitive->Material->Textures.empty())
                        material = getMaterialName(primitive->Material->Textures[0]->TextureResource);
                    obj << "usemtl " << material << "\n";

                    for (auto const& v : verts)
                        obj << "v " << v.Pos.X << " " << v.Pos.Y << " " << v.Pos.Z << "\n";
                    for (auto const& v : verts)
                        obj << "vt " << v.Uv.X << " " << (1.0f - v.Uv.Y) << "\n";
                    for (auto const& v : verts)
                        obj << "vn " << v.Normal.X << " " << v.Normal.Y << " " << v.Normal.Z << "\n";

                    std::size_t indexCount = primitive->IndexData.size() / 2;
                    auto getIndex = [&](std::size_t i) -> std::uint16_t {
                        return static_cast<std::uint16_t>(
                            primitive->IndexData[i * 2] |
                            (primitive->IndexData[i * 2 + 1] << 8));
                    };

                    auto emitTri = [&](std::uint16_t i0, std::uint16_t i1, std::uint16_t i2) {
                        std::size_t base = globalVertexBase;
                        obj << "f "
                            << (base + i0) << "/" << (base + i0) << "/" << (base + i0) << " "
                            << (base + i1) << "/" << (base + i1) << "/" << (base + i1) << " "
                            << (base + i2) << "/" << (base + i2) << "/" << (base + i2) << "\n";
                    };

                    std::size_t vertCount = verts.size();
                    if (vertCount == 0 || indexCount < 3)
                    {
                        globalVertexBase += verts.size();
                        continue;
                    }

                    auto countListInvalid = [&]() -> std::size_t {
                        std::size_t invalid = 0;
                        for (std::size_t i = 0; i + 2 < indexCount; i += 3)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                ++invalid;
                        }
                        return invalid;
                    };

                    auto countStripValid = [&]() -> std::size_t {
                        std::size_t valid = 0;
                        bool flip = false;
                        for (std::size_t i = 0; i + 2 < indexCount; ++i)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 == 0xFFFF || i1 == 0xFFFF || i2 == 0xFFFF)
                            {
                                flip = false;
                                continue;
                            }
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            if (i0 == i1 || i1 == i2 || i0 == i2)
                                continue;
                            ++valid;
                            flip = !flip;
                        }
                        return valid;
                    };

                    bool useStrip = (indexCount % 3) != 0;
                    std::size_t invalidList = countListInvalid();
                    if (!useStrip && invalidList > (indexCount / 3) / 20)
                    {
                        std::size_t stripValid = countStripValid();
                        if (stripValid > (indexCount / 3) / 2)
                            useStrip = true;
                    }

                    if (!useStrip)
                    {
                        for (std::size_t i = 0; i + 2 < indexCount; i += 3)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            emitTri(i0, i1, i2);
                        }
                    }
                    else
                    {
                        bool flip = false;
                        for (std::size_t i = 0; i + 2 < indexCount; ++i)
                        {
                            std::uint16_t i0 = getIndex(i + 0);
                            std::uint16_t i1 = getIndex(i + 1);
                            std::uint16_t i2 = getIndex(i + 2);
                            if (i0 == 0xFFFF || i1 == 0xFFFF || i2 == 0xFFFF)
                            {
                                flip = false;
                                continue;
                            }
                            if (i0 >= vertCount || i1 >= vertCount || i2 >= vertCount)
                                continue;
                            if (i0 == i1 || i1 == i2 || i0 == i2)
                                continue;
                            if (!flip)
                                emitTri(i0, i1, i2);
                            else
                                emitTri(i1, i0, i2);
                            flip = !flip;
                        }
                    }

                    globalVertexBase += verts.size();
                }
            };

            for (std::size_t i = 0; i < branchCount; ++i)
            {
                auto const& branch = tree->Branches[i];
                if (!branch)
                    continue;
                auto worldMatrix = computeWorld(static_cast<int>(i));

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
        }
    }

    if (materialNames.empty())
        materialNames[nullptr] = "default";

    for (auto const& entry : materialNames)
    {
        std::string const& name = entry.second;
        mtl << "newmtl " << name << "\n";
        mtl << "Ka 0 0 0\n";
        mtl << "Kd 1 1 1\n";
        mtl << "Ks 0 0 0\n";
        auto texIt = textureFiles.find(entry.first);
        if (texIt != textureFiles.end())
            mtl << "map_Kd " << texIt->second << "\n";
        mtl << "\n";
    }

    std::cout << "[CharmyBee] Exported forest OBJ to " << outputPath.string() << std::endl;
}

void CharmyBee::PollGlfwKeyInput()
{
    if (!_debugKeyInput || !_controller)
        return;

    auto* controller = _controller.get();
    GLFWwindow* window = controller->Window();
    if (window == nullptr)
        return;

    for (int key = GLFW_KEY_SPACE; key <= GLFW_KEY_LAST; ++key)
    {
        int state = glfwGetKey(window, key);
        bool down = state == GLFW_PRESS || state == GLFW_REPEAT;
        if (down != _glfwKeyStates[key])
        {
            _glfwKeyStates[key] = down;
            std::string name = DescribeGlfwKey(key);
            std::cout << "[CharmyBee][KeyInput] " << (down ? "Pressed " : "Released ")
                      << name << " (" << key << ")" << std::endl;
        }
    }

    for (int button = GLFW_MOUSE_BUTTON_LEFT; button <= GLFW_MOUSE_BUTTON_LAST; ++button)
    {
        int state = glfwGetMouseButton(window, button);
        bool down = state == GLFW_PRESS || state == GLFW_REPEAT;
        if (down != _glfwMouseButtonStates[button])
        {
            _glfwMouseButtonStates[button] = down;
            std::string desc;
            switch (button)
            {
            case GLFW_MOUSE_BUTTON_LEFT: desc = "Left"; break;
            case GLFW_MOUSE_BUTTON_RIGHT: desc = "Right"; break;
            case GLFW_MOUSE_BUTTON_MIDDLE: desc = "Middle"; break;
            default: desc = "Mouse-" + std::to_string(button); break;
            }
            std::cout << "[CharmyBee][MouseInput] " << (down ? "Pressed " : "Released ")
                      << desc << " (" << button << ")" << std::endl;
        }
    }
}


void CharmyBee::RenderMainDockWindow()
{
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(_width), static_cast<float>(_height)));
    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoBackground;

#ifdef ImGuiWindowFlags_NoDocking
    flags |= ImGuiWindowFlags_NoDocking;
#endif

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    bool open = true;
    bool show = ImGui::Begin("Main", &open, flags);
    ImGui::PopStyleVar();

    if (show)
    {
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Load SIF File..."))
                {
                    OpenSifFile();
                }
                if (ImGui::MenuItem("Unpack XPAC..."))
                {
                    UnpackXpac();
                }

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("View"))
            {
                if (ImGui::MenuItem("Forest hierarchy", nullptr, &_showForestHierarchyWindow))
                    UpdateForestBoxRenderer();
                ImGui::MenuItem("CppSLib Stuff", nullptr, &_showStuffWindow);
                ImGui::EndMenu();
            }

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3.0f, 4.0f));
            if (ImGui::BeginMenu("Nodes"))
            {
                DrawNodeCreationMenu();
                ImGui::EndMenu();
            }
            ImGui::PopStyleVar();

            ImVec2 cursorPos = ImGui::GetCursorPos();
            cursorPos.x += 10.0f;
            ImGui::SetCursorPos(cursorPos);
            if (ImGui::BeginTabBar("##modes"))
            {
                if (ImGui::BeginTabItem("Hierarchy"))
                {
                    float framerate = ImGui::GetIO().Framerate;
                    ImGui::Text("FPS %.1f", framerate);
                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Layout"))
                    ImGui::EndTabItem();

                if (ImGui::BeginTabItem("Navigation"))
                    ImGui::EndTabItem();

                ImGui::EndTabBar();
            }

            float width = ImGui::GetWindowWidth();
            float framerate = ImGui::GetIO().Framerate;
            ImGui::SetCursorPosX(width - 360);
            ImGui::Text("FPS %.1f", framerate);
            ImGui::EndMainMenuBar();
        }
    }

    ImGui::End();
}

void CharmyBee::RenderPanelWindow(char const* title, Editor::Panel::IEditorPanel* panel)
{
    ImGui::Begin(title);
    if (panel)
        panel->OnImGuiRender();
    ImGui::End();
}

void CharmyBee::Run()
{
    std::cout << "[CharmyBee] Initializing " << _title << " @ " << _width << "x" << _height << std::endl;

    _controller = std::make_unique<Graphics::ImGui::ImGuiController>(_width, _height);
    _assetPanel = std::make_unique<Editor::Panel::AssetPanel>();
    _scenePanel = std::make_unique<Editor::Panel::ScenePanel>();
    _inspectorPanel = std::make_unique<Editor::Panel::InspectorPanel>();

    OnLoad();

    auto previousTime = std::chrono::steady_clock::now();
    while (!_controller->ShouldClose())
    {
        auto loopStart = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = now - previousTime;
        previousTime = now;

        _blockSceneInput = false;

        _controller->SetPerFrameImGuiData(delta.count());
        auto newFrameStart = std::chrono::steady_clock::now();
        _controller->NewFrame();
        auto newFrameEnd = std::chrono::steady_clock::now();
        _imguiNewFrameMs = std::chrono::duration<float, std::milli>(newFrameEnd - newFrameStart).count();

        auto uiStart = std::chrono::steady_clock::now();
        auto dockStart = std::chrono::steady_clock::now();
        RenderMainDockWindow();
        auto dockEnd = std::chrono::steady_clock::now();
        _uiDockMs = std::chrono::duration<float, std::milli>(dockEnd - dockStart).count();

        auto racingStart = std::chrono::steady_clock::now();
        RenderRacingLineEditor();
        auto racingEnd = std::chrono::steady_clock::now();
        _uiRacingMs = std::chrono::duration<float, std::milli>(racingEnd - racingStart).count();

        auto stuffStart = std::chrono::steady_clock::now();
        RenderStuffWindow();
        auto stuffEnd = std::chrono::steady_clock::now();
        _uiStuffMs = std::chrono::duration<float, std::milli>(stuffEnd - stuffStart).count();

        auto forestStart = std::chrono::steady_clock::now();
        RenderForestHierarchyWindow();
        auto forestEnd = std::chrono::steady_clock::now();
        _uiForestMs = std::chrono::duration<float, std::milli>(forestEnd - forestStart).count();

        auto sifStart = std::chrono::steady_clock::now();
        RenderSifViewer();
        auto sifEnd = std::chrono::steady_clock::now();
        _uiSifMs = std::chrono::duration<float, std::milli>(sifEnd - sifStart).count();

        auto uiEnd = std::chrono::steady_clock::now();
        _uiBuildMs = std::chrono::duration<float, std::milli>(uiEnd - uiStart).count();

        auto animStart = std::chrono::steady_clock::now();
        UpdateAnimator(delta.count());
        auto animEnd = std::chrono::steady_clock::now();
        _animMs = std::chrono::duration<float, std::milli>(animEnd - animStart).count();
        _renderer.Render();
        auto imguiStart = std::chrono::steady_clock::now();
        _controller->Render();
        auto imguiEnd = std::chrono::steady_clock::now();
        _imguiRenderMs = std::chrono::duration<float, std::milli>(imguiEnd - imguiStart).count();

        auto swapStart = std::chrono::steady_clock::now();
        _controller->SwapBuffers();
        auto swapEnd = std::chrono::steady_clock::now();
        _swapMs = std::chrono::duration<float, std::milli>(swapEnd - swapStart).count();

        auto pollStart = std::chrono::steady_clock::now();
        _controller->PollEvents();
        auto pollEnd = std::chrono::steady_clock::now();
        _pollMs = std::chrono::duration<float, std::milli>(pollEnd - pollStart).count();
        auto inputStart = std::chrono::steady_clock::now();
        PollGlfwKeyInput();
        auto inputEnd = std::chrono::steady_clock::now();
        _inputMs = std::chrono::duration<float, std::milli>(inputEnd - inputStart).count();

        auto orbitStart = std::chrono::steady_clock::now();
        UpdateOrbitFromInput(delta.count());
        auto orbitEnd = std::chrono::steady_clock::now();
        _orbitMs = std::chrono::duration<float, std::milli>(orbitEnd - orbitStart).count();

        auto loopEnd = std::chrono::steady_clock::now();
        _frameLoopMs = std::chrono::duration<float, std::milli>(loopEnd - loopStart).count();
    }

    _controller->Dispose();
    if (!_forestHierarchy.empty())
        SaveForestHierarchyVisibility();
    SaveAnimatorSettings();
    std::cout << "[CharmyBee] Shutdown complete." << std::endl;
}

} // namespace SeEditor
