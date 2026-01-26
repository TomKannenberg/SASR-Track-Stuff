#include "SeEditor/UnityExport.hpp"

#include "SeEditor/LogicLoader.hpp"
#include "SeEditor/NavigationLoader.hpp"
#include "SeEditor/SifParser.hpp"
#include "SeEditor/Forest/ForestTypes.hpp"

#include "SlLib/Math/Vector.hpp"
#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Resources/Database/SlResourceRelocation.hpp"
#include "SlLib/Serialization/ResourceLoadContext.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <array>
#include <fstream>
#include <iomanip>
#include <optional>
#include <span>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <zlib.h>

using SlLib::Math::Vector2;
using SlLib::Math::Vector3;
using SlLib::Math::Vector4;

namespace SeEditor::UnityExport {

namespace {

std::string ToLowerHex(std::uint8_t b)
{
    const char* hex = "0123456789abcdef";
    std::string out;
    out.push_back(hex[(b >> 4) & 0xF]);
    out.push_back(hex[b & 0xF]);
    return out;
}

// Unity .meta GUIDs are 32 hex chars. We generate a stable GUID per asset path so scenes can reference assets
// without relying on Unity-side editor scripts.
std::string StableGuidForPath(std::filesystem::path const& p)
{
    auto s = p.generic_string();
    std::uint32_t h = 2166136261u; // FNV-1a 32-bit
    for (unsigned char c : s)
    {
        h ^= static_cast<std::uint32_t>(c);
        h *= 16777619u;
    }

    std::array<std::uint8_t, 16> bytes{};
    std::uint32_t x = h ^ 0xA5A5A5A5u;
    for (int i = 0; i < 16; ++i)
    {
        x ^= (x << 13);
        x ^= (x >> 17);
        x ^= (x << 5);
        bytes[i] = static_cast<std::uint8_t>((x >> ((i % 4) * 8)) & 0xFF);
    }

    std::string guid;
    guid.reserve(32);
    for (auto b : bytes)
        guid += ToLowerHex(b);
    return guid;
}

bool WriteTextFile(std::filesystem::path const& path, std::string const& text)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out)
        return false;
    out << text;
    return true;
}

bool WriteUnityModelMeta(std::filesystem::path const& assetPath, std::string const& guid)
{
    std::ostringstream meta;
    meta << "fileFormatVersion: 2\n";
    meta << "guid: " << guid << "\n";
    meta << "ModelImporter:\n";
    meta << "  serializedVersion: 22200\n";
    meta << "  internalIDToNameTable: []\n";
    meta << "  externalObjects: {}\n";
    meta << "  materials:\n";
    meta << "    materialImportMode: 2\n";
    meta << "    materialName: 0\n";
    meta << "    materialSearch: 1\n";
    meta << "    materialLocation: 1\n";
    meta << "  meshes:\n";
    meta << "    readable: 1\n";
    meta << "    importNormals: 2\n";
    meta << "    importTangents: 4\n";
    return WriteTextFile(assetPath.string() + ".meta", meta.str());
}

bool WriteUnityNativeMeta(std::filesystem::path const& assetPath, std::string const& guid)
{
    std::ostringstream meta;
    meta << "fileFormatVersion: 2\n";
    meta << "guid: " << guid << "\n";
    meta << "NativeFormatImporter:\n";
    meta << "  userData: \n";
    meta << "  assetBundleName: \n";
    meta << "  assetBundleVariant: \n";
    return WriteTextFile(assetPath.string() + ".meta", meta.str());
}

struct ObjVertex
{
    Vector3 Pos{};
    Vector3 Normal{};
    Vector2 Uv{};
};

bool StartsWith(std::span<const std::uint8_t> data, const char* magic4)
{
    return data.size() >= 4 &&
           data[0] == static_cast<std::uint8_t>(magic4[0]) &&
           data[1] == static_cast<std::uint8_t>(magic4[1]) &&
           data[2] == static_cast<std::uint8_t>(magic4[2]) &&
           data[3] == static_cast<std::uint8_t>(magic4[3]);
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

float HalfToFloat(std::uint16_t half)
{
    const std::uint32_t sign = (half & 0x8000u) << 16;
    const std::uint32_t exp = (half & 0x7C00u) >> 10;
    const std::uint32_t mant = (half & 0x03FFu);

    std::uint32_t out = 0;
    if (exp == 0)
    {
        if (mant == 0)
            out = sign;
        else
        {
            std::uint32_t m = mant;
            std::uint32_t e = 127 - 15 + 1;
            while ((m & 0x0400u) == 0)
            {
                m <<= 1;
                --e;
            }
            m &= 0x03FFu;
            out = sign | (e << 23) | (m << 13);
        }
    }
    else if (exp == 0x1Fu)
    {
        out = sign | 0x7F800000u | (mant << 13);
    }
    else
    {
        out = sign | ((exp + (127 - 15)) << 23) | (mant << 13);
    }

    float f;
    std::memcpy(&f, &out, sizeof(f));
    return f;
}

std::string SanitizeName(std::string name)
{
    for (char& c : name)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '.'))
            c = '_';
    }
    if (name.empty())
        name = "unnamed";
    return name;
}

std::string JsonEscape(std::string_view s)
{
    std::ostringstream out;
    for (char c : s)
    {
        switch (c)
        {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                out << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c
                    << std::dec << std::setfill(' ');
            }
            else
            {
                out << c;
            }
            break;
        }
    }
    return out.str();
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

    if (inflateInit(&inflater) != Z_OK)
        return {};

    std::vector<std::uint8_t> result;
    std::vector<std::uint8_t> buffer(1 << 14);

    int status = Z_OK;
    while (status != Z_STREAM_END)
    {
        inflater.next_out = buffer.data();
        inflater.avail_out = static_cast<decltype(inflater.avail_out)>(buffer.size());
        status = inflate(&inflater, 0);
        if (status != Z_OK && status != Z_STREAM_END)
        {
            inflateEnd(&inflater);
            return {};
        }
        const std::size_t have = buffer.size() - inflater.avail_out;
        result.insert(result.end(), buffer.begin(), buffer.begin() + have);
    }
    inflateEnd(&inflater);
    return result;
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

std::vector<std::uint8_t> LoadGpuDataForSif(const std::filesystem::path& sifPath)
{
    std::filesystem::path gpuPath = sifPath;
    if (gpuPath.extension() == ".sif")
        gpuPath.replace_extension(".zig");
    else if (gpuPath.extension() == ".zif")
        gpuPath.replace_extension(".zig");
    else if (gpuPath.extension() == ".sig")
        gpuPath.replace_extension(".zig");

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
        else
            data.clear();
    }
    else
    {
        StripLengthPrefixIfPresent(data);
    }

    return data;
}

std::vector<ObjVertex> DecodeVertexStream(SeEditor::Forest::SuRenderVertexStream const& stream)
{
    using namespace SeEditor::Forest;
    std::vector<ObjVertex> verts;
    if (stream.VertexCount <= 0 || stream.VertexStride <= 0 || stream.Stream.empty())
        return verts;

    auto readFloat = [&](std::size_t offset) -> float {
        if (offset + 4 > stream.Stream.size())
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, stream.Stream.data() + offset, sizeof(float));
        return v;
    };
    auto readU16 = [&](std::size_t offset) -> std::uint16_t {
        if (offset + 2 > stream.Stream.size())
            return 0;
        return static_cast<std::uint16_t>(stream.Stream[offset] | (stream.Stream[offset + 1] << 8));
    };
    auto readS16 = [&](std::size_t offset) -> std::int16_t { return static_cast<std::int16_t>(readU16(offset)); };

    verts.resize(static_cast<std::size_t>(stream.VertexCount));
    for (int i = 0; i < stream.VertexCount; ++i)
    {
        std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(stream.VertexStride);
        ObjVertex v{};
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
                v.Pos = {readFloat(posOff + 0), readFloat(posOff + 4), readFloat(posOff + 8)};
            }
            else if (attr.Usage == D3DDeclUsage::Normal)
            {
                if (attr.Type == D3DDeclType::Float3)
                    v.Normal = {readFloat(off + 0), readFloat(off + 4), readFloat(off + 8)};
                else if (attr.Type == D3DDeclType::Float16x4)
                    v.Normal = {HalfToFloat(readU16(off + 0)), HalfToFloat(readU16(off + 2)), HalfToFloat(readU16(off + 4))};
                else if (attr.Type == D3DDeclType::Short4N)
                    v.Normal = {readS16(off + 0) / 32767.0f, readS16(off + 2) / 32767.0f, readS16(off + 4) / 32767.0f};
            }
            else if (attr.Usage == D3DDeclUsage::TexCoord)
            {
                if (attr.Type == D3DDeclType::Float2)
                    v.Uv = {readFloat(off + 0), readFloat(off + 4)};
                else if (attr.Type == D3DDeclType::Float16x2)
                    v.Uv = {HalfToFloat(readU16(off + 0)), HalfToFloat(readU16(off + 2))};
            }
        }
        verts[static_cast<std::size_t>(i)] = v;
    }

    return verts;
}

std::vector<std::uint32_t> DecodePrimitiveIndices(SeEditor::Forest::SuRenderPrimitive const& primitive,
                                                  std::size_t vertexLimit)
{
    std::size_t indexCount = primitive.IndexData.size() / 2;
    if (primitive.NumIndices > 0)
        indexCount = std::min<std::size_t>(indexCount, static_cast<std::size_t>(primitive.NumIndices));
    else if (indexCount == 0)
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
            mode.Count = std::min<std::size_t>(mode.Count, static_cast<std::size_t>(primitive.NumIndices));
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

    IndexMode best = eval16(false);
    IndexMode cand16be = eval16(true);
    if (cand16be.Droppable < best.Droppable)
        best = cand16be;

    IndexMode cand32le = eval32(false);
    if (cand32le.Count > 0 && cand32le.Droppable < best.Droppable)
        best = cand32le;

    IndexMode cand32be = eval32(true);
    if (cand32be.Count > 0 && cand32be.Droppable < best.Droppable)
        best = cand32be;

    const bool use32Bit = best.Use32;
    const bool swapIndices = best.Swap;

    std::vector<std::uint32_t> rawIndices;
    rawIndices.reserve(best.Count);
    if (use32Bit)
    {
        for (std::size_t i = 0; i < best.Count; ++i)
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
            rawIndices.push_back(idx);
        }
    }

    const int primitiveType = primitive.Unknown_0x9c;
    const bool isStrip = primitiveType == 5 || (primitiveType != 4 && best.Restart > 0);

    std::vector<std::uint32_t> validIndices;
    if (isStrip)
    {
        validIndices.reserve(rawIndices.size());
        bool have0 = false;
        bool have1 = false;
        std::uint32_t i0 = 0;
        std::uint32_t i1 = 0;
        bool flip = false;
        for (std::uint32_t idx : rawIndices)
        {
            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
            {
                have0 = false;
                have1 = false;
                flip = false;
                continue;
            }
            if (idx >= vertexLimit)
                continue;
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
                    validIndices.push_back(i1);
                    validIndices.push_back(i0);
                    validIndices.push_back(idx);
                }
                else
                {
                    validIndices.push_back(i0);
                    validIndices.push_back(i1);
                    validIndices.push_back(idx);
                }
            }
            i0 = i1;
            i1 = idx;
            flip = !flip;
        }
    }
    else
    {
        validIndices.reserve(rawIndices.size());
        for (std::uint32_t idx : rawIndices)
        {
            if ((use32Bit && idx == 0xFFFFFFFFu) || (!use32Bit && idx == 0xFFFFu))
                continue;
            if (idx >= vertexLimit)
                continue;
            validIndices.push_back(idx);
        }
    }

    return validIndices;
}

bool WriteObj(const std::filesystem::path& path,
              std::string_view objectName,
              std::span<const ObjVertex> vertices,
              std::span<const std::uint32_t> indices)
{
    std::ofstream obj(path);
    if (!obj)
        return false;

    obj << "o " << SanitizeName(std::string(objectName)) << '\n';
    for (auto const& v : vertices)
        obj << "v " << v.Pos.X << ' ' << v.Pos.Y << ' ' << v.Pos.Z << '\n';
    for (auto const& v : vertices)
        obj << "vn " << v.Normal.X << ' ' << v.Normal.Y << ' ' << v.Normal.Z << '\n';
    for (auto const& v : vertices)
        obj << "vt " << v.Uv.X << ' ' << v.Uv.Y << '\n';

    const std::size_t vertexBase = 1;
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3)
    {
        obj << "f ";
        for (int v = 0; v < 3; ++v)
        {
            std::size_t idx = static_cast<std::size_t>(indices[i + v]);
            obj << vertexBase + idx << '/' << vertexBase + idx << '/' << vertexBase + idx;
            if (v < 2)
                obj << ' ';
        }
        obj << '\n';
    }

    return true;
}

bool ExportRenderMeshToObj(const std::filesystem::path& outPath,
                           SeEditor::Forest::SuRenderMesh const& mesh,
                           std::string_view objectName)
{
    if (mesh.Primitives.empty())
        return false;

    std::vector<ObjVertex> mergedVertices;
    std::vector<std::uint32_t> mergedIndices;
    for (auto const& prim : mesh.Primitives)
    {
        if (!prim || !prim->VertexStream)
            continue;
        auto verts = DecodeVertexStream(*prim->VertexStream);
        if (verts.empty())
            continue;
        auto idx = DecodePrimitiveIndices(*prim, verts.size());
        if (idx.empty())
            continue;
        std::uint32_t base = static_cast<std::uint32_t>(mergedVertices.size());
        mergedVertices.insert(mergedVertices.end(), verts.begin(), verts.end());
        for (auto i : idx)
            mergedIndices.push_back(base + i);
    }

    if (mergedVertices.empty() || mergedIndices.empty())
        return false;

    return WriteObj(outPath, objectName, mergedVertices, mergedIndices);
}

struct ObjMtlExportResult
{
    bool Success = false;
    std::size_t SubMeshCount = 0;
};

std::filesystem::path WriteTextureFile(const std::filesystem::path& texturesRoot,
                                       const std::string& name,
                                       const std::vector<std::uint8_t>& imageData,
                                       std::error_code& ec)
{
    if (imageData.empty())
        return {};

    std::string base = SanitizeName(name);
    if (base.empty())
        base = "tex";

    std::string ext = StartsWith(std::span<const std::uint8_t>(imageData.data(), imageData.size()), "DDS ") ? ".dds" : ".bin";
    std::filesystem::path outPath = texturesRoot / (base + ext);

    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec)
        return {};

    std::ofstream out(outPath, std::ios::binary | std::ios::trunc);
    if (!out)
        return {};
    out.write(reinterpret_cast<const char*>(imageData.data()), static_cast<std::streamsize>(imageData.size()));
    return outPath;
}

ObjMtlExportResult ExportTreeToObjMtl(const std::filesystem::path& objPath,
                                     const std::filesystem::path& mtlPath,
                                     const std::filesystem::path& texturesRoot,
                                     SeEditor::Forest::SuRenderTree const& tree,
                                     const SeEditor::Forest::SuRenderForest& forest,
                                     std::error_code& ec)
{
    ObjMtlExportResult result{};

    std::ofstream obj(objPath, std::ios::binary | std::ios::trunc);
    if (!obj)
        return result;
    std::ofstream mtl(mtlPath, std::ios::binary | std::ios::trunc);
    if (!mtl)
        return result;

    // Map texture resource ptr -> exported path
    std::unordered_map<const SeEditor::Forest::SuRenderTextureResource*, std::filesystem::path> texOutByPtr;
    texOutByPtr.reserve(forest.TextureResources.size());

    auto materialName = [](const std::shared_ptr<SeEditor::Forest::SuRenderMaterial>& mat) -> std::string {
        if (!mat)
            return "mat_default";
        if (!mat->Name.empty())
            return SanitizeName(mat->Name);
        return "mat_" + std::to_string(mat->Hash);
    };

    auto materialTexturePath = [&](const std::shared_ptr<SeEditor::Forest::SuRenderMaterial>& mat) -> std::filesystem::path {
        if (!mat)
            return {};
        for (auto const& t : mat->Textures)
        {
            if (!t || !t->TextureResource)
                continue;
            auto* key = t->TextureResource.get();
            auto it = texOutByPtr.find(key);
            if (it != texOutByPtr.end())
                return it->second;

            auto written = WriteTextureFile(texturesRoot, t->TextureResource->Name, t->TextureResource->ImageData, ec);
            if (!written.empty())
            {
                texOutByPtr[key] = written;
                return written;
            }
        }
        return {};
    };

    // Emit materials as we encounter them.
    std::unordered_map<std::string, bool> writtenMtl;

    obj << "mtllib " << mtlPath.filename().string() << "\n";
    obj << "o tree\n";

    std::size_t vBase = 1;
    std::size_t subMeshCount = 0;

    for (auto const& branch : tree.Branches)
    {
        if (!branch)
            continue;

        auto exportMesh = [&](const std::shared_ptr<SeEditor::Forest::SuRenderMesh>& mesh) {
            if (!mesh)
                return;
            for (auto const& prim : mesh->Primitives)
            {
                if (!prim || !prim->VertexStream)
                    continue;
                auto verts = DecodeVertexStream(*prim->VertexStream);
                if (verts.empty())
                    continue;
                auto idx = DecodePrimitiveIndices(*prim, verts.size());
                if (idx.empty())
                    continue;

                std::string mName = materialName(prim->Material);
                if (!writtenMtl[mName])
                {
                    writtenMtl[mName] = true;
                    mtl << "newmtl " << mName << "\n";
                    mtl << "Kd 1.000000 1.000000 1.000000\n";
                    auto texPath = materialTexturePath(prim->Material);
                    if (!texPath.empty())
                    {
                        auto rel = std::filesystem::relative(texPath, objPath.parent_path(), ec);
                        if (!ec)
                            mtl << "map_Kd " << rel.generic_string() << "\n";
                    }
                    mtl << "\n";
                }

                obj << "g " << mName << "\n";
                obj << "usemtl " << mName << "\n";
                ++subMeshCount;

                for (auto const& v : verts)
                    obj << "v " << v.Pos.X << ' ' << v.Pos.Y << ' ' << v.Pos.Z << "\n";
                for (auto const& v : verts)
                    obj << "vn " << v.Normal.X << ' ' << v.Normal.Y << ' ' << v.Normal.Z << "\n";
                for (auto const& v : verts)
                    obj << "vt " << v.Uv.X << ' ' << v.Uv.Y << "\n";

                for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
                {
                    obj << "f ";
                    for (int k = 0; k < 3; ++k)
                    {
                        std::size_t vi = vBase + static_cast<std::size_t>(idx[i + k]);
                        obj << vi << "/" << vi << "/" << vi;
                        if (k < 2)
                            obj << ' ';
                    }
                    obj << "\n";
                }
                vBase += verts.size();
            }
        };

        exportMesh(branch->Mesh);
        if (branch->Lod)
        {
            for (auto const& th : branch->Lod->Thresholds)
                if (th && th->Mesh)
                    exportMesh(th->Mesh);
        }
    }

    result.Success = true;
    result.SubMeshCount = subMeshCount;
    return result;
}

ObjMtlExportResult ExportBranchToObjMtl(const std::filesystem::path& objPath,
                                       const std::filesystem::path& mtlPath,
                                       const std::filesystem::path& texturesRoot,
                                       const std::shared_ptr<SeEditor::Forest::SuBranch>& branch,
                                       const SeEditor::Forest::SuRenderForest& forest,
                                       std::error_code& ec)
{
    ObjMtlExportResult result{};
    if (!branch)
        return result;

    std::ofstream obj(objPath, std::ios::binary | std::ios::trunc);
    if (!obj)
        return result;
    std::ofstream mtl(mtlPath, std::ios::binary | std::ios::trunc);
    if (!mtl)
        return result;

    std::unordered_map<const SeEditor::Forest::SuRenderTextureResource*, std::filesystem::path> texOutByPtr;
    texOutByPtr.reserve(forest.TextureResources.size());

    auto materialName = [](const std::shared_ptr<SeEditor::Forest::SuRenderMaterial>& mat) -> std::string {
        if (!mat)
            return "mat_default";
        if (!mat->Name.empty())
            return SanitizeName(mat->Name);
        return "mat_" + std::to_string(mat->Hash);
    };

    auto materialTexturePath = [&](const std::shared_ptr<SeEditor::Forest::SuRenderMaterial>& mat) -> std::filesystem::path {
        if (!mat)
            return {};
        for (auto const& t : mat->Textures)
        {
            if (!t || !t->TextureResource)
                continue;
            auto* key = t->TextureResource.get();
            auto it = texOutByPtr.find(key);
            if (it != texOutByPtr.end())
                return it->second;

            auto written = WriteTextureFile(texturesRoot, t->TextureResource->Name, t->TextureResource->ImageData, ec);
            if (!written.empty())
            {
                texOutByPtr[key] = written;
                return written;
            }
        }
        return {};
    };

    std::unordered_map<std::string, bool> writtenMtl;

    obj << "mtllib " << mtlPath.filename().string() << "\n";
    obj << "o branch\n";

    std::size_t vBase = 1;
    std::size_t subMeshCount = 0;

    auto exportMesh = [&](const std::shared_ptr<SeEditor::Forest::SuRenderMesh>& mesh) {
        if (!mesh)
            return;
        for (auto const& prim : mesh->Primitives)
        {
            if (!prim || !prim->VertexStream)
                continue;
            auto verts = DecodeVertexStream(*prim->VertexStream);
            if (verts.empty())
                continue;
            auto idx = DecodePrimitiveIndices(*prim, verts.size());
            if (idx.empty())
                continue;

            std::string mName = materialName(prim->Material);
            if (!writtenMtl[mName])
            {
                writtenMtl[mName] = true;
                mtl << "newmtl " << mName << "\n";
                mtl << "Kd 1.000000 1.000000 1.000000\n";
                auto texPath = materialTexturePath(prim->Material);
                if (!texPath.empty())
                {
                    auto rel = std::filesystem::relative(texPath, objPath.parent_path(), ec);
                    if (!ec)
                        mtl << "map_Kd " << rel.generic_string() << "\n";
                }
                mtl << "\n";
            }

            obj << "g " << mName << "\n";
            obj << "usemtl " << mName << "\n";
            ++subMeshCount;

            for (auto const& v : verts)
                obj << "v " << v.Pos.X << ' ' << v.Pos.Y << ' ' << v.Pos.Z << "\n";
            for (auto const& v : verts)
                obj << "vn " << v.Normal.X << ' ' << v.Normal.Y << ' ' << v.Normal.Z << "\n";
            for (auto const& v : verts)
                obj << "vt " << v.Uv.X << ' ' << v.Uv.Y << "\n";

            for (std::size_t i = 0; i + 2 < idx.size(); i += 3)
            {
                obj << "f ";
                for (int k = 0; k < 3; ++k)
                {
                    std::size_t vi = vBase + static_cast<std::size_t>(idx[i + k]);
                    obj << vi << "/" << vi << "/" << vi;
                    if (k < 2)
                        obj << ' ';
                }
                obj << "\n";
            }
            vBase += verts.size();
        }
    };

    exportMesh(branch->Mesh);
    if (branch->Lod)
    {
        for (auto const& th : branch->Lod->Thresholds)
            if (th && th->Mesh)
                exportMesh(th->Mesh);
    }

    result.Success = (subMeshCount > 0);
    result.SubMeshCount = subMeshCount;
    return result;
}

bool ExportCollisionToObj(const std::filesystem::path& outPath,
                          SeEditor::Forest::SuRenderTree const& tree)
{
    std::ofstream obj(outPath);
    if (!obj)
        return false;

    obj << "o collision\n";
    std::size_t vBase = 1;
    for (auto const& cm : tree.CollisionMeshes)
    {
        if (!cm)
            continue;
        for (auto const& tri : cm->Triangles)
        {
            if (!tri)
                continue;
            obj << "v " << tri->A.X << ' ' << tri->A.Y << ' ' << tri->A.Z << '\n';
            obj << "v " << tri->B.X << ' ' << tri->B.Y << ' ' << tri->B.Z << '\n';
            obj << "v " << tri->C.X << ' ' << tri->C.Y << ' ' << tri->C.Z << '\n';
            obj << "f " << vBase << ' ' << (vBase + 1) << ' ' << (vBase + 2) << '\n';
            vBase += 3;
        }
    }
    return true;
}

bool ParseCollisionMeshChunk(SeEditor::SifChunkInfo const& chunk,
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
        Vector3 v{readFloat(off + 0), readFloat(off + 4), readFloat(off + 8)};
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

bool TryLoadForestLibraryFromChunk(SeEditor::SifChunkInfo const& chunk,
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

std::string FormatVector3(float x, float y, float z)
{
    std::ostringstream stream;
    stream << "{x: " << std::fixed << std::setprecision(6) << x
           << ", y: " << y
           << ", z: " << z << "}";
    return stream.str();
}

std::string FormatQuaternion(float x, float y, float z, float w)
{
    std::ostringstream stream;
    stream << "{x: " << std::fixed << std::setprecision(6) << x
           << ", y: " << y
           << ", z: " << z
           << ", w: " << w << "}";
    return stream.str();
}

struct UnitySceneWriter
{
    struct PrefabInstance
    {
        int PrefabInstanceId = 0; // !u!1001
        int ParentTransformId = 0;
        std::string Name;
        std::string SourceGuid;
    };

    struct Node
    {
        int GameObjectId = 0;
        int TransformId = 0;
        int ParentTransformId = 0;
        std::vector<int> Children;
        std::string Name;
        Vector4 T{0, 0, 0, 0};
        Vector4 R{0, 0, 0, 1};
        Vector4 S{1, 1, 1, 1};
    };

    int NextFileId = 1;
    std::vector<Node> Nodes;
    std::vector<PrefabInstance> Prefabs;

    int AllocateId() { return NextFileId++; }

    int AddNode(std::string name, int parentTransformId, Vector4 t, Vector4 r, Vector4 s)
    {
        Node n;
        n.GameObjectId = AllocateId();
        n.TransformId = AllocateId();
        n.ParentTransformId = parentTransformId;
        n.Name = std::move(name);
        n.T = t;
        n.R = r;
        n.S = s;
        Nodes.push_back(std::move(n));
        return Nodes.back().TransformId;
    }

    void AddMeshPrefabInstance(int parentTransformId, std::string name, std::string sourceGuid)
    {
        PrefabInstance p;
        p.PrefabInstanceId = AllocateId();
        p.ParentTransformId = parentTransformId;
        p.Name = std::move(name);
        p.SourceGuid = std::move(sourceGuid);
        Prefabs.push_back(std::move(p));
    }

    void BuildChildLists()
    {
        std::unordered_map<int, std::size_t> byTransform;
        byTransform.reserve(Nodes.size());
        for (std::size_t i = 0; i < Nodes.size(); ++i)
            byTransform[Nodes[i].TransformId] = i;

        for (auto& n : Nodes)
        {
            if (n.ParentTransformId == 0)
                continue;
            auto it = byTransform.find(n.ParentTransformId);
            if (it == byTransform.end())
                continue;
            Nodes[it->second].Children.push_back(n.TransformId);
        }
    }

    bool Write(const std::filesystem::path& scenePath)
    {
        BuildChildLists();

        std::ofstream out(scenePath, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;

        out << "%YAML 1.1\n";
        out << "%TAG !u! tag:unity3d.com,2011:\n";

        // Minimal scene documents (matches UnityTooling defaults).
        auto writeDoc = [&](const char* tag, int fileId, const std::string& body) {
            out << "--- " << tag << " &" << fileId << "\n";
            out << body << "\n";
        };
        writeDoc("!u!29", AllocateId(), "Scene:\n  m_ObjectHideFlags: 0\n  m_PVSData: \n  m_QueryMode: 1\n  m_PVSObjectsArray: []\n  m_PVSPortalsArray: []\n  m_ViewCellSize: 1.000000");
        writeDoc("!u!127", AllocateId(), "GameManager:\n  m_ObjectHideFlags: 0");
        writeDoc("!u!157", AllocateId(), "LightmapSettings:\n  m_ObjectHideFlags: 0\n  m_LightProbeCloud: {fileID: 0}\n  m_Lightmaps: []\n  m_LightmapsMode: 1\n  m_BakedColorSpace: 0\n  m_UseDualLightmapsInForward: 0");
        writeDoc("!u!196", AllocateId(), "NavMeshSettings:\n  m_ObjectHideFlags: 0\n  m_BuildSettings:\n    cellSize: 0.200000\n    cellHeight: 0.100000\n    agentSlope: 45.000000\n    agentHeight: 1.800000\n    agentRadius: 0.400000\n    maxEdgeLength: 12\n    maxSimplificationError: 1.300000\n    regionMinSize: 8\n    regionMergeSize: 20\n    detailSampleDistance: 6.000000\n    detailSampleMaxError: 1.000000\n    accuratePlacement: 0\n  m_NavMesh: {fileID: 0}");

        // Now write GameObjects + Transforms.
        for (auto const& n : Nodes)
        {
            {
                std::ostringstream body;
                body << "Transform:\n";
                body << "  m_ObjectHideFlags: 0\n";
                body << "  m_PrefabParentObject: {fileID: 0}\n";
                body << "  m_PrefabInternal: {fileID: 0}\n";
                body << "  m_GameObject: {fileID: " << n.GameObjectId << "}\n";
                body << "  m_LocalRotation: " << FormatQuaternion(n.R.X, n.R.Y, n.R.Z, n.R.W) << "\n";
                body << "  m_LocalPosition: " << FormatVector3(n.T.X, n.T.Y, n.T.Z) << "\n";
                body << "  m_LocalScale: " << FormatVector3(n.S.X, n.S.Y, n.S.Z) << "\n";
                body << "  m_Children: [";
                for (std::size_t i = 0; i < n.Children.size(); ++i)
                {
                    if (i > 0)
                        body << ", ";
                    body << "{fileID: " << n.Children[i] << "}";
                }
                body << "]\n";
                body << "  m_Father: {fileID: " << n.ParentTransformId << "}\n";
                writeDoc("!u!4", n.TransformId, body.str());
            }
            {
                std::ostringstream body;
                body << "GameObject:\n";
                body << "  m_ObjectHideFlags: 0\n";
                body << "  m_PrefabParentObject: {fileID: 0}\n";
                body << "  m_PrefabInternal: {fileID: 0}\n";
                body << "  importerVersion: 3\n";
                body << "  m_Component:\n";
                body << "  - component: {fileID: " << n.TransformId << "}\n";
                body << "  m_Layer: 0\n";
                body << "  m_Name: " << n.Name << "\n";
                body << "  m_TagString: Untagged\n";
                body << "  m_Icon: {fileID: 0}\n";
                body << "  m_NavMeshLayer: 0\n";
                body << "  m_StaticEditorFlags: 0\n";
                body << "  m_IsActive: 1\n";
                writeDoc("!u!1", n.GameObjectId, body.str());
            }
        }

        // Prefab instances for meshes (tree OBJ imports show up as prefabs in the hierarchy).
        for (auto const& p : Prefabs)
        {
            std::ostringstream body;
            body << "PrefabInstance:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  serializedVersion: 2\n";
            body << "  m_Modification:\n";
            body << "    m_TransformParent: {fileID: " << p.ParentTransformId << "}\n";
            body << "    m_Modifications:\n";
            body << "    - target: {fileID: 100000, guid: " << p.SourceGuid << ", type: 3}\n";
            body << "      propertyPath: m_Name\n";
            body << "      value: " << p.Name << "\n";
            body << "      objectReference: {fileID: 0}\n";
            body << "    m_RemovedComponents: []\n";
            body << "    m_RemovedGameObjects: []\n";
            body << "  m_SourcePrefab: {fileID: 100100000, guid: " << p.SourceGuid << ", type: 3}\n";
            out << "--- !u!1001 &" << p.PrefabInstanceId << "\n";
            out << body.str() << "\n";
        }

        return true;
    }
};

struct UnityPrefabWriter
{
    struct Node
    {
        int GameObjectId = 0;
        int TransformId = 0;
        int ParentTransformId = 0;
        std::vector<int> Children;
        std::string Name;
        Vector4 T{0, 0, 0, 0};
        Vector4 R{0, 0, 0, 1};
        Vector4 S{1, 1, 1, 1};
    };

    struct PrefabInstance
    {
        int PrefabInstanceId = 0; // !u!1001
        int ParentTransformId = 0;
        std::string Name;
        std::string SourceGuid;
    };

    int NextFileId = 100001;
    std::vector<Node> Nodes;
    std::vector<PrefabInstance> PrefabInstances;

    int AllocateId() { return NextFileId++; }

    int AddNode(std::string name, int parentTransformId, Vector4 t, Vector4 r, Vector4 s)
    {
        Node n;
        n.GameObjectId = AllocateId();
        n.TransformId = AllocateId();
        n.ParentTransformId = parentTransformId;
        n.Name = std::move(name);
        n.T = t;
        n.R = r;
        n.S = s;
        Nodes.push_back(std::move(n));
        return Nodes.back().TransformId;
    }

    void AddMeshPrefabInstance(int parentTransformId, std::string name, std::string sourceGuid)
    {
        PrefabInstance p;
        p.PrefabInstanceId = AllocateId();
        p.ParentTransformId = parentTransformId;
        p.Name = std::move(name);
        p.SourceGuid = std::move(sourceGuid);
        PrefabInstances.push_back(std::move(p));
    }

    void BuildChildLists()
    {
        std::unordered_map<int, std::size_t> byTransform;
        byTransform.reserve(Nodes.size());
        for (std::size_t i = 0; i < Nodes.size(); ++i)
            byTransform[Nodes[i].TransformId] = i;

        for (auto& n : Nodes)
        {
            if (n.ParentTransformId == 0)
                continue;
            auto it = byTransform.find(n.ParentTransformId);
            if (it == byTransform.end())
                continue;
            Nodes[it->second].Children.push_back(n.TransformId);
        }
    }

    bool WritePrefab(const std::filesystem::path& prefabPath)
    {
        BuildChildLists();
        if (Nodes.empty())
            return false;

        std::ofstream out(prefabPath, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;

        out << "%YAML 1.1\n";
        out << "%TAG !u! tag:unity3d.com,2011:\n";

        const int prefabRootGo = Nodes[0].GameObjectId;

        out << "--- !u!1001 &100100000\n";
        out << "Prefab:\n";
        out << "  m_ObjectHideFlags: 1\n";
        out << "  serializedVersion: 2\n";
        out << "  m_Modification:\n";
        out << "    m_TransformParent: {fileID: 0}\n";
        out << "    m_Modifications: []\n";
        out << "    m_RemovedComponents: []\n";
        out << "  m_ParentPrefab: {fileID: 0}\n";
        out << "  m_RootGameObject: {fileID: " << prefabRootGo << "}\n";
        out << "  m_IsPrefabParent: 1\n";

        auto writeDoc = [&](const char* tag, int fileId, const std::string& body) {
            out << "--- " << tag << " &" << fileId << "\n";
            out << body << "\n";
        };

        for (auto const& n : Nodes)
        {
            {
                std::ostringstream body;
                body << "GameObject:\n";
                body << "  m_ObjectHideFlags: 0\n";
                body << "  m_PrefabParentObject: {fileID: 0}\n";
                body << "  m_PrefabInternal: {fileID: 100100000}\n";
                body << "  serializedVersion: 5\n";
                body << "  m_Component:\n";
                body << "  - component: {fileID: " << n.TransformId << "}\n";
                body << "  m_Layer: 0\n";
                body << "  m_Name: " << n.Name << "\n";
                body << "  m_TagString: Untagged\n";
                body << "  m_Icon: {fileID: 0}\n";
                body << "  m_NavMeshLayer: 0\n";
                body << "  m_StaticEditorFlags: 0\n";
                body << "  m_IsActive: 1\n";
                writeDoc("!u!1", n.GameObjectId, body.str());
            }
            {
                std::ostringstream body;
                body << "Transform:\n";
                body << "  m_ObjectHideFlags: 0\n";
                body << "  m_PrefabParentObject: {fileID: 0}\n";
                body << "  m_PrefabInternal: {fileID: 100100000}\n";
                body << "  m_GameObject: {fileID: " << n.GameObjectId << "}\n";
                body << "  m_LocalRotation: " << FormatQuaternion(n.R.X, n.R.Y, n.R.Z, n.R.W) << "\n";
                body << "  m_LocalPosition: " << FormatVector3(n.T.X, n.T.Y, n.T.Z) << "\n";
                body << "  m_LocalScale: " << FormatVector3(n.S.X, n.S.Y, n.S.Z) << "\n";
                body << "  m_Children: [";
                for (std::size_t i = 0; i < n.Children.size(); ++i)
                {
                    if (i > 0)
                        body << ", ";
                    body << "{fileID: " << n.Children[i] << "}";
                }
                body << "]\n";
                body << "  m_Father: {fileID: " << n.ParentTransformId << "}\n";
                writeDoc("!u!4", n.TransformId, body.str());
            }
        }

        for (auto const& p : PrefabInstances)
        {
            std::ostringstream body;
            body << "PrefabInstance:\n";
            body << "  m_ObjectHideFlags: 0\n";
            body << "  serializedVersion: 2\n";
            body << "  m_Modification:\n";
            body << "    m_TransformParent: {fileID: " << p.ParentTransformId << "}\n";
            body << "    m_Modifications:\n";
            body << "    - target: {fileID: 100000, guid: " << p.SourceGuid << ", type: 3}\n";
            body << "      propertyPath: m_Name\n";
            body << "      value: " << p.Name << "\n";
            body << "      objectReference: {fileID: 0}\n";
            body << "    m_RemovedComponents: []\n";
            body << "    m_RemovedGameObjects: []\n";
            body << "  m_SourcePrefab: {fileID: 100100000, guid: " << p.SourceGuid << ", type: 3}\n";
            writeDoc("!u!1001", p.PrefabInstanceId, body.str());
        }

        return true;
    }
};

} // namespace

std::filesystem::path FindUnityProjectRoot(std::filesystem::path const& startDir)
{
    std::error_code ec;
    std::filesystem::path cur = std::filesystem::weakly_canonical(startDir, ec);
    if (ec)
        cur = startDir;

    for (int i = 0; i < 8; ++i)
    {
        auto candidate = cur / "Unity";
        if (std::filesystem::exists(candidate / "Assets", ec) &&
            std::filesystem::exists(candidate / "ProjectSettings", ec))
        {
            return candidate;
        }
        if (!cur.has_parent_path())
            break;
        cur = cur.parent_path();
    }
    return {};
}

static bool WriteMinimalUnityPackages(const std::filesystem::path& unityRoot, std::string& error)
{
    std::error_code ec;
    const auto packagesDir = unityRoot / "Packages";
    std::filesystem::create_directories(packagesDir, ec);
    if (ec)
    {
        error = "Failed to create Packages/: " + ec.message();
        return false;
    }
    const auto manifest = packagesDir / "manifest.json";
    if (!std::filesystem::exists(manifest, ec))
    {
        std::ofstream out(manifest, std::ios::binary | std::ios::trunc);
        if (!out)
        {
            error = "Failed to write Packages/manifest.json";
            return false;
        }
        out << "{\n";
        out << "  \"dependencies\": {\n";
        out << "    \"com.unity.modules.assetbundle\": \"1.0.0\",\n";
        out << "    \"com.unity.modules.physics\": \"1.0.0\",\n";
        out << "    \"com.unity.modules.physics2d\": \"1.0.0\",\n";
        out << "    \"com.unity.modules.ui\": \"1.0.0\"\n";
        out << "  }\n";
        out << "}\n";
    }
    return true;
}

static std::filesystem::path FindTemplateRoot(std::filesystem::path const& preferredBase)
{
    std::error_code ec;
    // Prefer template next to the repo / working directory (NOT inside the user's export root).
    std::filesystem::path cur = std::filesystem::weakly_canonical(preferredBase, ec);
    if (ec)
        cur = preferredBase;

    for (int i = 0; i < 12; ++i)
    {
        const std::filesystem::path candidate = cur / "SSAR_MapTemplate";
        if (std::filesystem::exists(candidate, ec) && std::filesystem::is_directory(candidate, ec))
            return candidate;
        if (!cur.has_parent_path())
            break;
        cur = cur.parent_path();
    }
    return {};
}

static std::optional<std::filesystem::path> TryPrepareUnityProjectAtRoot(std::filesystem::path const& userRoot,
                                                                         std::string& error)
{
    std::error_code ec;
    const auto root = std::filesystem::weakly_canonical(userRoot, ec);
    const std::filesystem::path base = ec ? userRoot : root;

    const std::filesystem::path unity = base / "Unity";
    const std::filesystem::path assets = unity / "Assets";
    const std::filesystem::path projectSettings = unity / "ProjectSettings";

    if (std::filesystem::exists(assets, ec) && std::filesystem::exists(projectSettings, ec))
        return unity;

    std::filesystem::create_directories(assets, ec);
    std::filesystem::create_directories(projectSettings, ec);
    if (ec)
    {
        error = "Failed to create Unity project folders: " + ec.message();
        return std::nullopt;
    }

    // Try to locate SSAR_MapTemplate near the running app (cwd), not under the user's export root.
    const std::filesystem::path templateDir = FindTemplateRoot(std::filesystem::current_path());
    if (!templateDir.empty())
    {
        for (auto const& entry : std::filesystem::directory_iterator(templateDir, ec))
        {
            if (ec)
            {
                error = "Failed to iterate template: " + ec.message();
                return std::nullopt;
            }
            const auto name = entry.path().filename().string();
            if (_stricmp(name.c_str(), "Assets") == 0)
                continue;

            const std::filesystem::path dst = unity / entry.path().filename();
            std::filesystem::copy(entry.path(),
                                  dst,
                                  std::filesystem::copy_options::recursive | std::filesystem::copy_options::update_existing,
                                  ec);
            if (ec)
            {
                error = "Failed to copy template item: " + entry.path().string() + " -> " + dst.string() + " (" + ec.message() + ")";
                return std::nullopt;
            }
        }
    }
    else
    {
        // Do not hard-fail; create a minimal Unity project layout so export works even without the template.
        if (!WriteMinimalUnityPackages(unity, error))
            return std::nullopt;
    }

    if (!std::filesystem::exists(assets, ec) || !std::filesystem::exists(projectSettings, ec))
    {
        error = "Unity project setup incomplete under: " + unity.string();
        return std::nullopt;
    }
    return unity;
}

ExportResult ExportSifToUnity(std::filesystem::path const& sifPath,
                             std::filesystem::path const& unityProjectRoot)
{
    ExportResult result;

    if (!std::filesystem::exists(sifPath))
    {
        result.Error = "Input not found: " + sifPath.string();
        return result;
    }
    // Accept either a real Unity project root OR a user-selected "root folder" and create/use `<root>/Unity`.
    std::filesystem::path unityRoot = unityProjectRoot;
    if (!(std::filesystem::exists(unityRoot / "Assets") && std::filesystem::exists(unityRoot / "ProjectSettings")))
    {
        std::string prepError;
        auto prepared = TryPrepareUnityProjectAtRoot(unityProjectRoot, prepError);
        if (!prepared)
        {
            result.Error = prepError;
            return result;
        }
        unityRoot = *prepared;
    }

    std::ifstream file(sifPath, std::ios::binary);
    if (!file)
    {
        result.Error = "Failed to open input: " + sifPath.string();
        return result;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)), {});
    std::vector<std::uint8_t> data(buffer.begin(), buffer.end());

    std::string error;
    auto parsed = SeEditor::ParseSifFile(std::span<const std::uint8_t>(data.data(), data.size()), error);
    if (!parsed)
    {
        result.Error = "SIF parse error: " + error;
        return result;
    }

    auto gpuData = LoadGpuDataForSif(sifPath);
    std::span<const std::uint8_t> gpuSpan;
    if (!gpuData.empty())
        gpuSpan = std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

    const std::filesystem::path exportRoot = unityRoot / "Assets" / (sifPath.stem().string() + "_SIF");
    const std::filesystem::path meshesRoot = exportRoot / "Meshes";
    const std::filesystem::path texturesRoot = exportRoot / "Textures";
    const std::filesystem::path prefabsRoot = exportRoot / "Prefabs";
    const std::filesystem::path jsonPath = exportRoot / "export.json";
    const std::filesystem::path scenePath = exportRoot / (sifPath.stem().string() + ".unity");
    const std::filesystem::path collisionPath = exportRoot / "collision.obj";
    const std::filesystem::path subbranchRoot = exportRoot / "SuBranches";

    std::error_code ec;
    std::filesystem::create_directories(meshesRoot, ec);
    std::filesystem::create_directories(texturesRoot, ec);
    std::filesystem::create_directories(subbranchRoot, ec);
    std::filesystem::create_directories(prefabsRoot, ec);
    if (ec)
    {
        result.Error = "Failed to create output dirs: " + ec.message();
        return result;
    }

    struct ForestSource
    {
        std::string name;
        std::shared_ptr<SeEditor::Forest::ForestLibrary> library;
    };
    std::vector<ForestSource> forests;
    for (auto const& chunk : parsed->Chunks)
    {
        if (chunk.TypeValue != 0x45524F46) // 'FORE'
            continue;
        std::shared_ptr<SeEditor::Forest::ForestLibrary> lib;
        if (!TryLoadForestLibraryFromChunk(chunk, gpuSpan, lib, error))
            continue;
        forests.push_back({chunk.Name, std::move(lib)});
    }

    SlLib::SumoTool::Siff::LogicData logic;
    SeEditor::LogicProbeInfo logicProbe{};
    std::string logicError;
    bool hasLogic = SeEditor::LoadLogicFromSifChunks(parsed->Chunks, logic, logicProbe, logicError);

    SlLib::SumoTool::Siff::Navigation navigation;
    SeEditor::NavigationProbeInfo navProbe{};
    std::string navError;
    bool hasNav = SeEditor::LoadNavigationFromSifChunks(parsed->Chunks, navigation, navProbe, navError);

    // Export a single collision mesh for the whole SIF from COLI chunk (Unity-readable OBJ).
    {
        auto it = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
                               [](SeEditor::SifChunkInfo const& c) { return c.TypeValue == 0x494C4F43; }); // 'COLI'
        if (it != parsed->Chunks.end())
        {
            std::vector<SlLib::Math::Vector3> vertices;
            std::vector<std::array<int, 3>> triangles;
            std::string colError;
            if (ParseCollisionMeshChunk(*it, vertices, triangles, colError))
            {
                std::ofstream obj(collisionPath, std::ios::binary | std::ios::trunc);
                if (!obj)
                {
                    result.Error = "Failed to write collision: " + collisionPath.string();
                    return result;
                }
                obj << "o collision\n";
                for (auto const& v : vertices)
                    obj << "v " << v.X << ' ' << v.Y << ' ' << v.Z << '\n';
                for (auto const& t : triangles)
                    obj << "f " << (t[0] + 1) << ' ' << (t[1] + 1) << ' ' << (t[2] + 1) << '\n';
            }
            else
            {
                // Still create the file so Unity has a stable reference.
                std::ofstream obj(collisionPath, std::ios::binary | std::ios::trunc);
                if (obj)
                    obj << "o collision\n";
            }
        }
        else
        {
            std::ofstream obj(collisionPath, std::ios::binary | std::ios::trunc);
            if (obj)
                obj << "o collision\n";
        }
    }
    // Stable GUID so scenes can reference the collision mesh if needed.
    WriteUnityModelMeta(collisionPath, StableGuidForPath(collisionPath));

    std::ostringstream json;
    json << "{\n";
    json << "  \"sif\": \"" << JsonEscape(sifPath.filename().string()) << "\",\n";
    json << "  \"collision\": \"" << JsonEscape(std::filesystem::relative(collisionPath, exportRoot).generic_string()) << "\",\n";
    json << "  \"forests\": [\n";

    bool firstForestEntry = true;
    for (auto const& forestLib : forests)
    {
        if (!forestLib.library)
            continue;
        for (auto const& entry : forestLib.library->Forests)
        {
            if (!entry.Forest)
                continue;

            if (!firstForestEntry)
                json << ",\n";
            firstForestEntry = false;

            const std::string forestName = entry.Name.empty() ? ("forest_" + std::to_string(entry.Hash)) : entry.Name;
            const std::filesystem::path forestMeshDir = meshesRoot / SanitizeName(forestName);
            std::filesystem::create_directories(forestMeshDir, ec);

            json << "    {\n";
            json << "      \"name\": \"" << JsonEscape(forestName) << "\",\n";
            json << "      \"hash\": " << entry.Hash << ",\n";
            json << "      \"trees\": [\n";

            bool firstTree = true;
            for (std::size_t treeIdx = 0; treeIdx < entry.Forest->Trees.size(); ++treeIdx)
            {
                auto const& tree = entry.Forest->Trees[treeIdx];
                if (!tree)
                    continue;

                if (!firstTree)
                    json << ",\n";
                firstTree = false;

                const std::filesystem::path treeMeshDir = forestMeshDir / ("Tree" + std::to_string(treeIdx));
                std::filesystem::create_directories(treeMeshDir, ec);

                json << "        {\n";
                json << "          \"index\": " << treeIdx << ",\n";
                json << "          \"hash\": " << tree->Hash << ",\n";
                // Tree-level combined mesh is no longer exported; the tree is composed from per-branch meshes.
                json << "          \"mesh\": null,\n";
                json << "          \"branches\": [\n";

                bool firstBranch = true;
                for (std::size_t branchIdx = 0; branchIdx < tree->Branches.size(); ++branchIdx)
                {
                    auto const& branch = tree->Branches[branchIdx];
                    if (!branch)
                        continue;

                    if (!firstBranch)
                        json << ",\n";
                    firstBranch = false;

                    Vector4 t{};
                    Vector4 r{};
                    Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
                    if (branchIdx < tree->Translations.size())
                        t = tree->Translations[branchIdx];
                    if (branchIdx < tree->Rotations.size())
                        r = tree->Rotations[branchIdx];
                    if (branchIdx < tree->Scales.size())
                        s = tree->Scales[branchIdx];

                    std::string branchDisplayName = branch->Name.empty() ? ("Branch_" + std::to_string(branchIdx)) : branch->Name;
                    std::optional<std::string> meshRel;
                    std::vector<std::string> lodMeshes;

                    // Always dump a raw-ish branch file for repacking later.
                    const std::filesystem::path subDir =
                        subbranchRoot / SanitizeName(forestName) / ("Tree" + std::to_string(treeIdx));
                    std::filesystem::create_directories(subDir, ec);
                    const std::filesystem::path subPath = subDir / ("branch_" + std::to_string(branchIdx) + ".subranch.json");
                    {
                        std::vector<int> children;
                        int child = branch->Child;
                        while (child >= 0 && static_cast<std::size_t>(child) < tree->Branches.size())
                        {
                            children.push_back(child);
                            auto const& cb = tree->Branches[static_cast<std::size_t>(child)];
                            if (!cb)
                                break;
                            child = cb->Sibling;
                        }

                        std::ofstream sb(subPath, std::ios::binary | std::ios::trunc);
                        if (sb)
                        {
                            sb << "{\n";
                            sb << "  \"index\": " << branchIdx << ",\n";
                            sb << "  \"name\": \"" << JsonEscape(branch->Name) << "\",\n";
                            sb << "  \"flags\": " << branch->Flags << ",\n";
                            sb << "  \"parent\": " << branch->Parent << ",\n";
                            sb << "  \"child\": " << branch->Child << ",\n";
                            sb << "  \"sibling\": " << branch->Sibling << ",\n";
                            sb << "  \"children\": [";
                            for (std::size_t ci = 0; ci < children.size(); ++ci)
                            {
                                if (ci > 0)
                                    sb << ", ";
                                sb << children[ci];
                            }
                            sb << "],\n";
                            sb << "  \"t\": [" << t.X << ", " << t.Y << ", " << t.Z << "],\n";
                            sb << "  \"r\": [" << r.X << ", " << r.Y << ", " << r.Z << ", " << r.W << "],\n";
                            sb << "  \"s\": [" << s.X << ", " << s.Y << ", " << s.Z << "],\n";
                            sb << "  \"mesh\": null,\n";
                            sb << "  \"lodMeshes\": []\n";
                            sb << "}\n";
                        }
                    }
                    const std::string subRel = std::filesystem::relative(subPath, exportRoot).generic_string();

                    std::optional<std::string> exportedBranchMeshRel;
                    {
                        const std::filesystem::path branchDir = treeMeshDir / "SuBranches";
                        std::filesystem::create_directories(branchDir, ec);

                        const std::filesystem::path brObj = branchDir / ("branch_" + std::to_string(branchIdx) + ".obj");
                        const std::filesystem::path brMtl = branchDir / ("branch_" + std::to_string(branchIdx) + ".mtl");
                        auto exp = ExportBranchToObjMtl(brObj, brMtl, texturesRoot, branch, *entry.Forest, ec);
                        if (exp.Success)
                        {
                            exportedBranchMeshRel = std::filesystem::relative(brObj, exportRoot).generic_string();
                            WriteUnityModelMeta(brObj, StableGuidForPath(brObj));
                        }
                    }

                    json << "            {\n";
                    json << "              \"index\": " << branchIdx << ",\n";
                    json << "              \"name\": \"" << JsonEscape(branch->Name) << "\",\n";
                    json << "              \"parent\": " << branch->Parent << ",\n";
                    json << "              \"child\": " << branch->Child << ",\n";
                    json << "              \"sibling\": " << branch->Sibling << ",\n";
                    json << "              \"t\": [" << t.X << ", " << t.Y << ", " << t.Z << "],\n";
                    json << "              \"r\": [" << r.X << ", " << r.Y << ", " << r.Z << ", " << r.W << "],\n";
                    json << "              \"s\": [" << s.X << ", " << s.Y << ", " << s.Z << "],\n";
                    json << "              \"mesh\": " << (exportedBranchMeshRel ? ("\"" + JsonEscape(*exportedBranchMeshRel) + "\"") : "null") << ",\n";
                    json << "              \"subranch\": \"" << JsonEscape(subRel) << "\"";
                    json << "\n";
                    json << "            }";
                }

                json << "\n          ],\n";

                json << "          \"collision\": null\n";
                json << "        }";
            }

            json << "\n      ]\n";
            json << "    }";
        }
    }
    json << "\n  ],\n";

    json << "  \"logic\": {\n";
    json << "    \"present\": " << (hasLogic ? "true" : "false") << ",\n";
    json << "    \"error\": \"" << JsonEscape(hasLogic ? "" : logicError) << "\",\n";
    json << "    \"nameHash\": " << (hasLogic ? logic.NameHash : 0) << ",\n";
    json << "    \"version\": " << (hasLogic ? logic.LogicVersion : 0) << ",\n";

    json << "    \"triggers\": [\n";
    if (hasLogic)
    {
        bool first = true;
        for (auto const& trig : logic.Triggers)
        {
            if (!trig)
                continue;
            if (!first)
                json << ",\n";
            first = false;
            json << "      {\"nameHash\": " << trig->NameHash
                 << ", \"flags\": " << trig->Flags
                 << ", \"pos\": [" << trig->Position.X << ", " << trig->Position.Y << ", " << trig->Position.Z << "]"
                 << ", \"normal\": [" << trig->Normal.X << ", " << trig->Normal.Y << ", " << trig->Normal.Z << "]"
                 << ", \"v0\": [" << trig->Vertex0.X << ", " << trig->Vertex0.Y << ", " << trig->Vertex0.Z << "]"
                 << ", \"v1\": [" << trig->Vertex1.X << ", " << trig->Vertex1.Y << ", " << trig->Vertex1.Z << "]"
                 << ", \"v2\": [" << trig->Vertex2.X << ", " << trig->Vertex2.Y << ", " << trig->Vertex2.Z << "]"
                 << ", \"v3\": [" << trig->Vertex3.X << ", " << trig->Vertex3.Y << ", " << trig->Vertex3.Z << "]}";
        }
    }
    json << "\n    ],\n";

    json << "    \"locators\": [\n";
    if (hasLogic)
    {
        bool first = true;
        for (auto const& loc : logic.Locators)
        {
            if (!loc)
                continue;
            if (!first)
                json << ",\n";
            first = false;
            json << "      {\"groupHash\": " << loc->GroupNameHash
                 << ", \"nameHash\": " << loc->LocatorNameHash
                 << ", \"meshForestHash\": " << loc->MeshForestNameHash
                 << ", \"meshTreeHash\": " << loc->MeshTreeNameHash
                 << ", \"setupObjectHash\": " << loc->SetupObjectNameHash
                 << ", \"flags\": " << loc->Flags
                 << ", \"pos\": [" << loc->PositionAsFloats.X << ", " << loc->PositionAsFloats.Y << ", " << loc->PositionAsFloats.Z << "]"
                 << ", \"rot\": [" << loc->RotationAsFloats.X << ", " << loc->RotationAsFloats.Y << ", " << loc->RotationAsFloats.Z << ", " << loc->RotationAsFloats.W << "]}";
        }
    }
    json << "\n    ]\n";
    json << "  }\n";

    json << ",\n  \"navigation\": {\n";
    json << "    \"present\": " << (hasNav ? "true" : "false") << ",\n";
    json << "    \"error\": \"" << JsonEscape(hasNav ? "" : navError) << "\",\n";
    json << "    \"version\": " << (hasNav ? navProbe.Version : 0) << ",\n";
    json << "    \"baseOffset\": " << (hasNav ? static_cast<unsigned long long>(navProbe.BaseOffset) : 0ULL) << ",\n";
    json << "    \"waypoints\": [\n";
    if (hasNav)
    {
        std::unordered_map<const SlLib::SumoTool::Siff::NavData::NavWaypoint*, int> wpIndex;
        wpIndex.reserve(navigation.Waypoints.size());
        for (std::size_t i = 0; i < navigation.Waypoints.size(); ++i)
        {
            auto const& wp = navigation.Waypoints[i];
            if (wp)
                wpIndex[wp.get()] = static_cast<int>(i);
        }

        bool firstWp = true;
        for (std::size_t i = 0; i < navigation.Waypoints.size(); ++i)
        {
            auto const& wp = navigation.Waypoints[i];
            if (!wp)
                continue;
            if (!firstWp)
                json << ",\n";
            firstWp = false;

            json << "      {\"index\": " << i
                 << ", \"name\": \"" << JsonEscape(wp->Name)
                 << "\", \"flags\": " << wp->Flags
                 << ", \"pos\": [" << wp->Pos.X << ", " << wp->Pos.Y << ", " << wp->Pos.Z << "]"
                 << ", \"dir\": [" << wp->Dir.X << ", " << wp->Dir.Y << ", " << wp->Dir.Z << "]"
                 << ", \"up\": [" << wp->Up.X << ", " << wp->Up.Y << ", " << wp->Up.Z << "]"
                 << ", \"to\": [";

            bool firstTo = true;
            for (auto const& link : wp->ToLinks)
            {
                if (!link || !link->To)
                    continue;
                auto it = wpIndex.find(link->To);
                if (it == wpIndex.end())
                    continue;
                if (!firstTo)
                    json << ", ";
                firstTo = false;
                json << it->second;
            }
            json << "]}";
        }
    }
    json << "\n    ]\n";
    json << "  }\n";

    json << "}\n";

    std::ofstream out(jsonPath, std::ios::binary | std::ios::trunc);
    if (!out)
    {
        result.Error = "Failed to write " + jsonPath.string();
        return result;
    }
    out << json.str();

    // Build a Unity scene that instantiates one prefab per tree (prefab contains SuBranch hierarchy + meshes).
    UnitySceneWriter scene;
    int sceneRoot = scene.AddNode(SanitizeName(sifPath.stem().string()), 0, {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 1});
    int navRoot = 0;
    int navWpRoot = 0;
    if (hasNav)
    {
        navRoot = scene.AddNode("Navigation", sceneRoot, {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 1});
        navWpRoot = scene.AddNode("Waypoints", navRoot, {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 1});
        for (std::size_t i = 0; i < navigation.Waypoints.size(); ++i)
        {
            auto const& wp = navigation.Waypoints[i];
            if (!wp)
                continue;
            Vector4 t{wp->Pos.X, wp->Pos.Y, wp->Pos.Z, 0.0f};
            scene.AddNode("WP_" + std::to_string(i), navWpRoot, t, {0, 0, 0, 1}, {1, 1, 1, 1});
        }
    }
    for (auto const& forestLib : forests)
    {
        if (!forestLib.library)
            continue;
        for (auto const& entry : forestLib.library->Forests)
        {
            if (!entry.Forest)
                continue;
            const std::string forestName = entry.Name.empty() ? ("forest_" + std::to_string(entry.Hash)) : entry.Name;
            int forestNode = scene.AddNode(SanitizeName(forestName), sceneRoot, {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 1});
            for (std::size_t treeIdx = 0; treeIdx < entry.Forest->Trees.size(); ++treeIdx)
            {
                auto const& tree = entry.Forest->Trees[treeIdx];
                if (!tree)
                    continue;

                const std::filesystem::path forestMeshDir = meshesRoot / SanitizeName(forestName);
                const std::filesystem::path treeMeshDir = forestMeshDir / ("Tree" + std::to_string(treeIdx));
                const std::filesystem::path treePrefabDir = prefabsRoot / SanitizeName(forestName);
                std::filesystem::create_directories(treePrefabDir, ec);
                const std::filesystem::path treePrefabPath = treePrefabDir / ("Tree_" + std::to_string(treeIdx) + ".prefab");

                UnityPrefabWriter prefab;
                int prefabRoot = prefab.AddNode("Tree_" + std::to_string(treeIdx), 0, {0, 0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1, 1});
                std::vector<int> branchTransformByIndex(tree->Branches.size(), 0);
                for (std::size_t branchIdx = 0; branchIdx < tree->Branches.size(); ++branchIdx)
                {
                    auto const& branch = tree->Branches[branchIdx];
                    if (!branch)
                        continue;
                    Vector4 t{};
                    Vector4 r{};
                    Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
                    if (branchIdx < tree->Translations.size())
                        t = tree->Translations[branchIdx];
                    if (branchIdx < tree->Rotations.size())
                        r = tree->Rotations[branchIdx];
                    if (branchIdx < tree->Scales.size())
                        s = tree->Scales[branchIdx];
                    const std::string name = branch->Name.empty() ? ("Branch_" + std::to_string(branchIdx)) : branch->Name;
                    int parentNode = prefabRoot;
                    if (branch->Parent >= 0 && static_cast<std::size_t>(branch->Parent) < branchTransformByIndex.size())
                    {
                        int p = branchTransformByIndex[static_cast<std::size_t>(branch->Parent)];
                        if (p != 0)
                            parentNode = p;
                    }
                    int branchNode = prefab.AddNode(SanitizeName(name), parentNode, t, r, s);
                    branchTransformByIndex[branchIdx] = branchNode;

                    const std::filesystem::path brObj = treeMeshDir / "SuBranches" / ("branch_" + std::to_string(branchIdx) + ".obj");
                    if (std::filesystem::exists(brObj))
                        prefab.AddMeshPrefabInstance(branchNode, "Mesh", StableGuidForPath(brObj));
                }

                if (prefab.WritePrefab(treePrefabPath))
                    WriteUnityNativeMeta(treePrefabPath, StableGuidForPath(treePrefabPath));

                scene.AddMeshPrefabInstance(forestNode, "Tree_" + std::to_string(treeIdx), StableGuidForPath(treePrefabPath));
            }
        }
    }
    if (!scene.Write(scenePath))
    {
        result.Error = "Failed to write scene: " + scenePath.string();
        return result;
    }

    result.Success = true;
    result.ExportJsonPath = jsonPath;
    result.ScenePath = scenePath;
    return result;
}

} // namespace SeEditor::UnityExport
