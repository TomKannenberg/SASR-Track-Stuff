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
#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Utilities/SlUtil.hpp"
#include "Forest/ForestArchive.hpp"

#include <SlLib/Excel/ExcelData.hpp>
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

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(chunk.Relocations.size());
    for (auto offset : chunk.Relocations)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        std::span<const std::uint8_t>(chunk.Data.data(), chunk.Data.size()),
        gpuData,
        std::move(relocations));
    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_xbox360("x360", true, false, 0);
    context.Platform = chunk.BigEndian ? &s_xbox360 : &s_win32;

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
                    cpu.Vertices.reserve(verts.size() * 8);
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

    // Orbit controls (right-drag rotate, scroll zoom) when hovering scene.
    ImGuiIO& io = ImGui::GetIO();
    float dt = io.DeltaTime > 0.0f ? io.DeltaTime : 1.0f / 60.0f;
    bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem |
                                          ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    if (hovered && io.MouseWheel != 0.0f)
    {
        _orbitDistance *= std::pow(0.9f, io.MouseWheel);
    }

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

    auto isDown = [&](int key) {
        int state = glfwGetKey(window, key);
        return state == GLFW_PRESS || state == GLFW_REPEAT;
    };

    float rotSpeed = 1.8f;
    float moveSpeed = _movementSpeed;

    if (isDown(GLFW_KEY_J)) _orbitYaw   -= rotSpeed * delta;
    if (isDown(GLFW_KEY_L)) _orbitYaw   += rotSpeed * delta;
    if (isDown(GLFW_KEY_I)) _orbitPitch -= rotSpeed * delta;
    if (isDown(GLFW_KEY_K)) _orbitPitch += rotSpeed * delta;

    if (isDown(GLFW_KEY_Z)) _movementSpeed /= 1.05f;
    if (isDown(GLFW_KEY_X)) _movementSpeed *= 1.05f;
    _movementSpeed = std::max(_movementSpeed, 0.01f);

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
        if (isMouseDown(GLFW_MOUSE_BUTTON_LEFT) || isMouseDown(GLFW_MOUSE_BUTTON_RIGHT))
        {
            SlLib::Math::Vector2 current = cursorPos;
            if (_mouseOrbitTracking)
            {
                SlLib::Math::Vector2 delta = {current.X - _mouseOrbitLastPos.X, current.Y - _mouseOrbitLastPos.Y};
                constexpr float mouseSensitivity = 0.01f;
                _orbitYaw   -= delta.X * mouseSensitivity;
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

    // Camera-forward points toward the target; right is orthogonal in camera space.
    SlLib::Math::Vector3 forward{-cp * cy, -sp, -cp * sy};
    SlLib::Math::Vector3 right{sy, 0.0f, -cy};

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
            deltaVec = deltaVec * (moveSpeed * delta * std::max(1.0f, _orbitDistance * 0.2f));
            _orbitOffset = _orbitOffset + deltaVec;
        }

    _orbitPitch = std::clamp(_orbitPitch, -1.4f, 1.4f);
    _orbitDistance = std::max(0.3f, _orbitDistance);

    _renderer.SetOrbitCamera(_orbitYaw, _orbitPitch, _orbitDistance, _orbitTarget + _orbitOffset);
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
                            }
                            ImGui::SameLine();
                            if (ImGui::Checkbox("Draw", &_drawForestMeshes))
                                UpdateForestMeshRendering();
                            ImGui::SameLine();
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
                            ImGui::SameLine();
                            if (ImGui::Checkbox("Draw", &_drawNavigation))
                                UpdateDebugLines();
                            if (_sifNavigationTool)
                            {
                                ImGui::SameLine();
                                if (ImGui::Checkbox("Waypoints", &_drawNavigationWaypoints))
                                    UpdateDebugLines();
                                if (_drawNavigationWaypoints)
                                {
                                    ImGui::SameLine();
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
                            ImGui::SameLine();
                            if (ImGui::Checkbox("Draw", &_drawLogic))
                            {
                                UpdateDebugLines();
                                UpdateForestMeshRendering();
                            }
                            ImGui::SameLine();
                            if (ImGui::Checkbox("Triggers", &_drawLogicTriggers))
                                UpdateDebugLines();
                            ImGui::SameLine();
                            if (ImGui::Checkbox("Locators", &_drawLogicLocators))
                            {
                                UpdateDebugLines();
                                UpdateForestMeshRendering();
                            }
                            if (_drawLogicLocators)
                            {
                                ImGui::SameLine();
                                ImGui::SetNextItemWidth(80.0f);
                                if (ImGui::DragFloat("Size", &_logicLocatorBoxSize, 0.1f, 0.2f, 50.0f, "%.1f"))
                                    UpdateDebugLines();
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
                    if (_forestBoxLayers.empty())
                    {
                        ImGui::Text("No forest data loaded yet.");
                    }
                    else
                    {
                        bool hierarchyChanged = false;
                        for (auto& layer : _forestBoxLayers)
                            hierarchyChanged |= RenderForestBoxLayer(layer);
                        if (hierarchyChanged)
                        {
                            UpdateForestBoxRenderer();
                            UpdateForestMeshRendering();
                        }
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
                else
                {
                    ImGui::TextDisabled("No hierarchy for this chunk.");
                }
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
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
                ImGui::BulletText("%s", file.filename().string().c_str());

            ImGui::TreePop();
        }
    }
    else
    {
        ImGui::BulletText("%s", label.c_str());
    }
}

void CharmyBee::RenderStuffSifVirtualTree(std::filesystem::path const& root)
{
    std::filesystem::path xpacRoot = root / "xpac";
    std::error_code ec;
    if (!std::filesystem::exists(xpacRoot, ec))
        return;

    if (!ImGui::TreeNodeEx("SIF Files", ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
        return;

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
        std::string xpacName = xpacDir.filename().string();
        if (!ImGui::TreeNodeEx(xpacName.c_str(), ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth))
            continue;

        std::vector<std::filesystem::path> sifs;
        for (auto const& entry : std::filesystem::recursive_directory_iterator(xpacDir, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;
            if (_stricmp(entry.path().extension().string().c_str(), ".sif") == 0)
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
                if (_stricmp(seg.c_str(), "_Resource") == 0)
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

            ImGui::PushID(sifPath.string().c_str());
            ImGui::Selectable(label.c_str());
            if (ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Load SIF"))
                    LoadSifFile(sifPath);
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }

        ImGui::TreePop();
    }

    ImGui::TreePop();
}

void CharmyBee::RenderStuffWindow()
{
    if (!_showStuffWindow)
        return;

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
        }
    }
    if (!_xpacStatus.empty())
        ImGui::TextWrapped("%s", _xpacStatus.c_str());
    if (ImGui::Button("Unpack XPAC..."))
        UnpackXpac();
    if (ImGui::Button("Nuke Stuff Folder"))
        _confirmNukeStuff = true;

    ImGui::Separator();
    ImGui::BeginChild("StuffTree", ImVec2(0.0f, 0.0f), true);
    RenderStuffTreeNode(root);
    RenderStuffSifVirtualTree(root);
    ImGui::EndChild();

    ImGui::End();

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

        _xpacBusy = false;
    });
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
        if (_stricmp(path.extension().string().c_str(), ".sif") == 0)
            fallback.replace_extension(".zif");
        else if (_stricmp(path.extension().string().c_str(), ".sig") == 0)
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

        try
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
    _renderer.SetForestMeshes({});
    _renderer.SetDrawForestMeshes(false);
    _drawForestMeshes = false;
    _forestLibrary.reset();
    _allForestMeshes.clear();

    auto it = std::find_if(_sifChunks.begin(), _sifChunks.end(),
        [](SifChunkInfo const& c) { return c.TypeValue == MakeTypeCode('F', 'O', 'R', 'E'); });

    if (it == _sifChunks.end())
        return;

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
    static SlLib::Resources::Database::SlPlatform s_xbox360("x360", true, false, 0);
    SlLib::Serialization::ResourceLoadContext context(cpuData, gpuData);
    bool isBigEndian = chunk.BigEndian;
    context.Platform = isBigEndian ? &s_xbox360 : &s_win32;

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

    std::vector<Renderer::SlRenderer::ForestCpuMesh> meshes;
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

                    SlLib::Math::Matrix4x4 normalMatrix = worldMatrix;
                    normalMatrix(0, 3) = 0.0f;
                    normalMatrix(1, 3) = 0.0f;
                    normalMatrix(2, 3) = 0.0f;

                    for (auto& v : verts)
                    {
                        SlLib::Math::Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                        auto transformed = SlLib::Math::Transform(worldMatrix, pos4);
                        v.Pos = {transformed.X, transformed.Y, transformed.Z};
                        treeInfo.Bounds.Include(v.Pos);
                        forestState.Include(v.Pos);

                        SlLib::Math::Vector4 n4{v.Normal.X, v.Normal.Y, v.Normal.Z, 0.0f};
                        auto nT = SlLib::Math::Transform(normalMatrix, n4);
                        v.Normal = SlLib::Math::normalize({nT.X, nT.Y, nT.Z});
                    }

                    Renderer::SlRenderer::ForestCpuMesh cpu;
                    cpu.Vertices.reserve(verts.size() * 8);
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
                    meshes.push_back(std::move(cpu));
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
        _drawForestMeshes = true;
        _forestLibrary = std::move(library);
        std::cout << "[CharmyBee] Forest meshes loaded: " << _forestLibrary->Forests.size() << " forests." << std::endl;
    }
    else
    {
        _allForestMeshes.clear();
    }

    _forestBoxLayers = std::move(layers);
    LoadForestVisibility();
    UpdateForestBoxRenderer();
    UpdateForestMeshRendering();
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
    static SlLib::Resources::Database::SlPlatform s_xbox360("x360", true, false, 0);
    context.Platform = bigEndian ? &s_xbox360 : &s_win32;

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
            for (std::size_t v = 0; v + 7 < transformed.Vertices.size(); v += 8)
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
                auto v0 = SlLib::Math::Vector3{trigger->Vertex0.X, trigger->Vertex0.Y, trigger->Vertex0.Z};
                auto v1 = SlLib::Math::Vector3{trigger->Vertex1.X, trigger->Vertex1.Y, trigger->Vertex1.Z};
                auto v2 = SlLib::Math::Vector3{trigger->Vertex2.X, trigger->Vertex2.Y, trigger->Vertex2.Z};
                auto v3 = SlLib::Math::Vector3{trigger->Vertex3.X, trigger->Vertex3.Y, trigger->Vertex3.Z};
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v0, v1, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v1, v2, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v2, v3, color});
                logicLines.push_back(Renderer::SlRenderer::DebugLine{v3, v0, color});
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
        _renderer.SetTriggerBoxes({});
        _renderer.SetDrawTriggerBoxes(false);
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

    if (_drawForestMeshes && !_allForestMeshes.empty())
    {
        std::vector<Renderer::SlRenderer::ForestCpuMesh> filtered;
        filtered.reserve(_allForestMeshes.size());

        auto gatherMeshes = [&](auto&& self, ForestBoxLayer const& layer) -> void {
            if (!layer.Visible)
                return;

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

        if (!filtered.empty())
        {
            combined.reserve(filtered.size() + _logicLocatorMeshes.size());
            combined.insert(combined.end(), filtered.begin(), filtered.end());
        }
    }

    if (_drawLogic && _drawLogicLocators && !_logicLocatorMeshes.empty())
    {
        if (combined.empty())
            combined.reserve(_logicLocatorMeshes.size());
        combined.insert(combined.end(), _logicLocatorMeshes.begin(), _logicLocatorMeshes.end());
    }

    bool hasVisible = !combined.empty();
    _renderer.SetForestMeshes(std::move(combined));
    _renderer.SetDrawForestMeshes(hasVisible);
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
                    ImGui::TextWrapped("Hierarchy and definition panels stay in their own window to the side.");
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
            ImGui::SetCursorPosX(width - 100);
            ImGui::Text("(%.1f FPS)", framerate);
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
        auto now = std::chrono::steady_clock::now();
        std::chrono::duration<float> delta = now - previousTime;
        previousTime = now;

        _controller->SetPerFrameImGuiData(delta.count());
        _controller->NewFrame();

        RenderMainDockWindow();
        RenderRacingLineEditor();
        RenderStuffWindow();

        RenderSifViewer();

        _renderer.Render();
        _controller->Render();
        _controller->SwapBuffers();
        _controller->PollEvents();
        PollGlfwKeyInput();

        UpdateOrbitFromInput(delta.count());
    }

    _controller->Dispose();
    std::cout << "[CharmyBee] Shutdown complete." << std::endl;
}

} // namespace SeEditor
