#include "SeEditor/Forest/ForestArchive.hpp"
#include "Forest/ForestTypes.hpp"
#include "SifParser.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/Resources/Database/SlPlatform.hpp>
#include <SlLib/Resources/Database/SlResourceRelocation.hpp>
#include <SlLib/Serialization/ResourceLoadContext.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace SeEditor;
using namespace SlLib::Math;

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
        return sign ? -std::ldexp(m, -14) : std::ldexp(m, -14);
    }
    if (exp == 31)
        return mant ? std::numeric_limits<float>::quiet_NaN()
                    : (sign ? -std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::infinity());

    float m = 1.0f + mant / 1024.0f;
    return sign ? -std::ldexp(m, exp - 15) : std::ldexp(m, exp - 15);
}

struct ObjVertex
{
    Vector3 Pos{};
    Vector3 Normal{};
    Vector2 Uv{};
};

std::vector<ObjVertex> DecodeVertex(SeEditor::Forest::SuRenderVertexStream const& stream)
{
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
            using Forest::D3DDeclType;
            using Forest::D3DDeclUsage;

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
                {
                    v.Normal = {readFloat(off + 0), readFloat(off + 4), readFloat(off + 8)};
                }
                else if (attr.Type == D3DDeclType::Float16x4)
                {
                    v.Normal = {HalfToFloat(readU16(off + 0)),
                                HalfToFloat(readU16(off + 2)),
                                HalfToFloat(readU16(off + 4))};
                }
                else if (attr.Type == D3DDeclType::Short4N)
                {
                    v.Normal = {readU16(off + 0) / 32767.0f,
                                readU16(off + 2) / 32767.0f,
                                readU16(off + 4) / 32767.0f};
                }
            }
            else if (attr.Usage == D3DDeclUsage::TexCoord)
            {
                if (attr.Type == D3DDeclType::Float2)
                {
                    v.Uv = {readFloat(off + 0), readFloat(off + 4)};
                }
                else if (attr.Type == D3DDeclType::Float16x2)
                {
                    v.Uv = {HalfToFloat(readU16(off + 0)), HalfToFloat(readU16(off + 2))};
                }
            }
        }
        verts[static_cast<std::size_t>(i)] = v;
    }

    return verts;
}

Matrix4x4 BuildLocalMatrix(Vector4 t, Vector4 r, Vector4 s)
{
    Quaternion q{r.X, r.Y, r.Z, r.W};
    Matrix4x4 rot = CreateFromQuaternion(q);
    Matrix4x4 scale{};
    scale(0, 0) = s.X;
    scale(1, 1) = s.Y;
    scale(2, 2) = s.Z;
    scale(3, 3) = 1.0f;
    Matrix4x4 local = Multiply(rot, scale);
    local(0, 3) = t.X;
    local(1, 3) = t.Y;
    local(2, 3) = t.Z;
    local(3, 3) = 1.0f;
    return local;
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

struct MeshOutput
{
    std::string Name;
    std::string MaterialName;
    std::shared_ptr<SeEditor::Forest::SuRenderMaterial> Material;
    std::vector<ObjVertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

void CompactMesh(MeshOutput& mesh)
{
    if (mesh.Vertices.empty() || mesh.Indices.empty())
        return;

    std::vector<std::uint8_t> used(mesh.Vertices.size(), 0u);
    for (std::uint32_t idx : mesh.Indices)
    {
        if (idx < used.size())
            used[idx] = 1u;
    }

    std::vector<std::uint32_t> remap(mesh.Vertices.size(), std::numeric_limits<std::uint32_t>::max());
    std::vector<ObjVertex> compact;
    compact.reserve(mesh.Vertices.size());
    for (std::size_t i = 0; i < mesh.Vertices.size(); ++i)
    {
        if (used[i])
        {
            remap[i] = static_cast<std::uint32_t>(compact.size());
            compact.push_back(mesh.Vertices[i]);
        }
    }

    if (compact.size() == mesh.Vertices.size())
        return;

    for (std::uint32_t& idx : mesh.Indices)
    {
        if (idx < remap.size() && remap[idx] != std::numeric_limits<std::uint32_t>::max())
            idx = remap[idx];
    }
    mesh.Vertices.swap(compact);
}

void WriteDefaultWhitePng(std::filesystem::path const& dir)
{
    static const unsigned char kPng[] = {
        0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A,
        0x00,0x00,0x00,0x0D,0x49,0x48,0x44,0x52,
        0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x01,
        0x08,0x02,0x00,0x00,0x00,0x90,0x77,0x53,
        0xDE,0x00,0x00,0x00,0x0A,0x49,0x44,0x41,
        0x54,0x08,0xD7,0x63,0xF8,0xCF,0xC0,0x00,
        0x00,0x04,0x03,0x01,0x00,0x18,0xDD,0x8D,
        0xB1,0x00,0x00,0x00,0x00,0x49,0x45,0x4E,
        0x44,0xAE,0x42,0x60,0x82
    };
    std::filesystem::path pngPath = dir / "default_white.png";
    if (std::filesystem::exists(pngPath))
        return;
    std::ofstream png(pngPath, std::ios::binary);
    if (!png)
        return;
    png.write(reinterpret_cast<char const*>(kPng), sizeof(kPng));
}

using namespace SeEditor::Forest;

bool TryParseForestArchive(std::span<const std::uint8_t> input,
                           std::vector<std::uint8_t>& cpuData,
                           std::vector<std::uint32_t>& relocations,
                           std::vector<std::uint8_t>& gpuData,
                           bool& bigEndian)
{
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

float CoverageRatio(std::vector<std::uint32_t> const& indices, std::size_t vertexCount)
{
    if (indices.size() < 3u || vertexCount == 0u)
        return 0.0f;
    std::vector<std::uint8_t> used(vertexCount, 0u);
    std::size_t unique = 0u;
    for (std::uint32_t idx : indices)
    {
        if (idx >= vertexCount)
            continue;
        if (used[idx] == 0u)
        {
            used[idx] = 1u;
            ++unique;
        }
    }
    return static_cast<float>(unique) / static_cast<float>(vertexCount);
}

constexpr std::uint32_t kForestTag = 0x45524F46; // 'FORE'

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsLikelyPs3Path(std::filesystem::path const& inputPath)
{
    std::string name = inputPath.filename().string();
    bool hasAlpha = false;
    for (char c : name)
    {
        if (!std::isalpha(static_cast<unsigned char>(c)))
            continue;
        hasAlpha = true;
        if (std::isupper(static_cast<unsigned char>(c)))
            return false;
    }
    return hasAlpha;
}

bool TryExtractForestFromSif(std::filesystem::path const& inputPath,
                             std::span<const std::uint8_t> rawCpuInput,
                             std::vector<std::uint8_t>& cpuData,
                             std::vector<std::uint32_t>& relocations,
                             std::vector<std::uint8_t>& gpuData,
                             bool& bigEndian,
                             std::string& error)
{
    auto parsed = ParseSifFile(rawCpuInput, error);
    if (!parsed)
        return false;

    auto forestIt = std::find_if(parsed->Chunks.begin(), parsed->Chunks.end(),
        [](SifChunkInfo const& chunk) { return chunk.TypeValue == kForestTag; });
    if (forestIt == parsed->Chunks.end())
    {
        error = "No Forest chunk found inside SIF.";
        return false;
    }

    cpuData = forestIt->Data;
    relocations = forestIt->Relocations;
    bigEndian = forestIt->BigEndian;
    gpuData.clear();

    std::filesystem::path gpuPath = inputPath;
    gpuPath.replace_extension(".sig");
    if (!std::filesystem::exists(gpuPath))
    {
        std::filesystem::path alt = inputPath;
        alt.replace_extension(".SIG");
        if (std::filesystem::exists(alt))
            gpuPath = std::move(alt);
    }

    if (std::filesystem::exists(gpuPath))
    {
        std::ifstream gpuFile(gpuPath, std::ios::binary);
        if (!gpuFile)
        {
            error = "Failed to open paired SIG file: " + gpuPath.string();
            return false;
        }
        gpuData.assign(std::istreambuf_iterator<char>(gpuFile), {});
    }

    return true;
}

// Attempt to locate the corresponding PC SIF/SIG for a PS3 asset and load all texture resources.
std::unordered_map<std::string, std::vector<std::uint8_t>> LoadPcTextureFallback(std::filesystem::path const& ps3Path)
{
    std::unordered_map<std::string, std::vector<std::uint8_t>> textures;
    auto stem = ps3Path.stem().string();
    if (stem.empty())
        return textures;
    auto capitalizeStem = [&]() {
        std::string s = stem;
        if (!s.empty())
            s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
        return s;
    };

    std::vector<std::filesystem::path> candidates;
    if (ps3Path.string().find("PS3_resource\\racers") != std::string::npos ||
        ps3Path.string().find("PS3_resource/racers") != std::string::npos)
    {
        auto base = ps3Path;
        for (int i = 0; i < 2; ++i)
            base = base.parent_path();
        candidates.push_back(base / "Racers" / "_Resource" / "_Racers" / (capitalizeStem() + ".sif"));
        candidates.push_back(base / "Racers" / "_Resource" / "_Racers" / (stem + ".sif"));
    }
    else if (ps3Path.string().find("PS3_resource\\tracks") != std::string::npos ||
             ps3Path.string().find("PS3_resource/tracks") != std::string::npos)
    {
        auto base = ps3Path;
        for (int i = 0; i < 2; ++i)
            base = base.parent_path();
        candidates.push_back(base / "oTracks" / "_Resource" / "_Tracks" / (capitalizeStem() + ".sif"));
        candidates.push_back(base / "oTracks" / "_Resource" / "_Tracks" / (stem + ".sif"));
    }

    std::filesystem::path pcSif;
    for (auto const& c : candidates)
    {
        if (std::filesystem::exists(c))
        {
            pcSif = c;
            break;
        }
    }
    if (pcSif.empty())
        return textures;

    std::vector<std::uint8_t> cpuData;
    std::vector<std::uint8_t> gpuData;
    std::vector<std::uint32_t> relocs;
    bool be = false;
    std::string error;
    {
        std::vector<std::uint8_t> raw;
        std::ifstream f(pcSif, std::ios::binary);
        if (!f)
            return textures;
        raw.assign(std::istreambuf_iterator<char>(f), {});
        if (!TryExtractForestFromSif(pcSif, raw, cpuData, relocs, gpuData, be, error))
            return textures;
    }

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocObjs;
    relocObjs.reserve(relocs.size());
    for (auto r : relocs)
        relocObjs.push_back({static_cast<int>(r), 0});

    SlLib::Serialization::ResourceLoadContext ctx(
        cpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(cpuData.data(), cpuData.size()),
        gpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(gpuData.data(), gpuData.size()),
        std::move(relocObjs));
    SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
    ctx.Platform = &win32;
    ctx.Version = 0;

    SeEditor::Forest::ForestLibrary lib;
    try
    {
        lib.Load(ctx);
    }
    catch (...)
    {
        return textures;
    }

    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };

    for (auto const& fe : lib.Forests)
    {
        if (!fe.Forest)
            continue;
        for (auto const& tr : fe.Forest->TextureResources)
        {
            if (!tr || tr->ImageData.empty())
                continue;
            std::string name = tr->Name.empty() ? "" : lower(std::filesystem::path(tr->Name).filename().string());
            if (name.empty())
                continue;
            if (textures.find(name) == textures.end())
                textures.emplace(std::move(name), tr->ImageData);
        }
    }
    return textures;
}

bool ReadBEU16(std::span<const std::uint8_t> data, std::size_t offset, std::uint16_t& out)
{
    if (offset + 2 > data.size())
        return false;
    out = static_cast<std::uint16_t>((data[offset] << 8) | data[offset + 1]);
    return true;
}

bool ReadBEI16(std::span<const std::uint8_t> data, std::size_t offset, std::int16_t& out)
{
    std::uint16_t v = 0;
    if (!ReadBEU16(data, offset, v))
        return false;
    out = static_cast<std::int16_t>(v);
    return true;
}

bool ReadBEU32(std::span<const std::uint8_t> data, std::size_t offset, std::uint32_t& out)
{
    if (offset + 4 > data.size())
        return false;
    out = (static_cast<std::uint32_t>(data[offset]) << 24) |
          (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
          static_cast<std::uint32_t>(data[offset + 3]);
    return true;
}

bool ReadBEFloat(std::span<const std::uint8_t> data, std::size_t offset, float& out)
{
    std::uint32_t raw = 0;
    if (!ReadBEU32(data, offset, raw))
        return false;
    std::memcpy(&out, &raw, sizeof(out));
    return true;
}

bool ReadBEFloat3(std::span<const std::uint8_t> data, std::size_t offset, Vector3& out)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    if (!ReadBEFloat(data, offset + 0, x) ||
        !ReadBEFloat(data, offset + 4, y) ||
        !ReadBEFloat(data, offset + 8, z))
    {
        return false;
    }
    out = {x, y, z};
    return true;
}

bool ReadBEFloat4(std::span<const std::uint8_t> data, std::size_t offset, Vector4& out)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;
    if (!ReadBEFloat(data, offset + 0, x) ||
        !ReadBEFloat(data, offset + 4, y) ||
        !ReadBEFloat(data, offset + 8, z) ||
        !ReadBEFloat(data, offset + 12, w))
    {
        return false;
    }
    out = {x, y, z, w};
    return true;
}

std::string ReadCString(std::span<const std::uint8_t> data, std::uint32_t address)
{
    if (static_cast<std::size_t>(address) >= data.size())
        return {};

    std::size_t cursor = static_cast<std::size_t>(address);
    std::string out;
    while (cursor < data.size())
    {
        char c = static_cast<char>(data[cursor++]);
        if (c == '\0')
            break;
        out.push_back(c);
    }
    return out;
}

Matrix4x4 IdentityMatrix()
{
    Matrix4x4 m{};
    m(0, 0) = 1.0f;
    m(1, 1) = 1.0f;
    m(2, 2) = 1.0f;
    m(3, 3) = 1.0f;
    return m;
}

Vector4 NormalizeQuaternion(Vector4 r)
{
    float length = std::sqrt(r.X * r.X + r.Y * r.Y + r.Z * r.Z + r.W * r.W);
    if (!std::isfinite(length) || length < 1.0e-8f)
        return {0.0f, 0.0f, 0.0f, 1.0f};
    float invLength = 1.0f / length;
    return {r.X * invLength, r.Y * invLength, r.Z * invLength, r.W * invLength};
}

float SanitizeScale(float value)
{
    if (!std::isfinite(value))
        return 1.0f;
    float absValue = std::abs(value);
    if (absValue < 1.0e-6f || absValue > 100.0f)
        return 1.0f;
    return value;
}

Vector2 DecodePs3UvTail(std::span<const std::uint8_t> streamData,
                        std::size_t streamOffset,
                        std::uint32_t index,
                        int stride)
{
    if (stride < 4)
        return {};

    std::size_t uvOffset = streamOffset +
        static_cast<std::size_t>(index) * static_cast<std::size_t>(stride) +
        static_cast<std::size_t>(stride - 4);

    std::uint16_t uRaw = 0;
    std::uint16_t vRaw = 0;
    if (!ReadBEU16(streamData, uvOffset + 0, uRaw) || !ReadBEU16(streamData, uvOffset + 2, vRaw))
        return {};

    Vector2 uv{HalfToFloat(uRaw), HalfToFloat(vRaw)};
    if (!std::isfinite(uv.X) || !std::isfinite(uv.Y) ||
        std::abs(uv.X) > 1000.0f || std::abs(uv.Y) > 1000.0f)
    {
        return {};
    }
    return uv;
}

std::size_t FilterPs3Triangles(MeshOutput& mesh)
{
    if (mesh.Indices.size() < 3u || mesh.Vertices.size() < 3u)
        return 0;

    auto edgeLength = [&](std::uint32_t ia, std::uint32_t ib) -> float {
        if (ia >= mesh.Vertices.size() || ib >= mesh.Vertices.size())
            return std::numeric_limits<float>::infinity();
        Vector3 const& a = mesh.Vertices[ia].Pos;
        Vector3 const& b = mesh.Vertices[ib].Pos;
        float dx = a.X - b.X;
        float dy = a.Y - b.Y;
        float dz = a.Z - b.Z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    std::size_t triCount = mesh.Indices.size() / 3u;
    std::size_t sampleTriCount = std::min<std::size_t>(triCount, 256u);
    std::vector<float> sampleEdges;
    sampleEdges.reserve(sampleTriCount * 3u);
    for (std::size_t ti = 0; ti < sampleTriCount; ++ti)
    {
        std::uint32_t i0 = mesh.Indices[ti * 3u + 0u];
        std::uint32_t i1 = mesh.Indices[ti * 3u + 1u];
        std::uint32_t i2 = mesh.Indices[ti * 3u + 2u];
        float l0 = edgeLength(i0, i1);
        float l1 = edgeLength(i1, i2);
        float l2 = edgeLength(i2, i0);
        if (std::isfinite(l0))
            sampleEdges.push_back(l0);
        if (std::isfinite(l1))
            sampleEdges.push_back(l1);
        if (std::isfinite(l2))
            sampleEdges.push_back(l2);
    }
    if (sampleEdges.empty())
        return 0;

    std::nth_element(sampleEdges.begin(),
                     sampleEdges.begin() + static_cast<std::ptrdiff_t>(sampleEdges.size() / 2u),
                     sampleEdges.end());
    float median = sampleEdges[sampleEdges.size() / 2u];
    float longEdgeThreshold = std::max(100.0f, median * 20.0f);
    constexpr float kAspectThreshold = 30.0f;
    constexpr float kMinEdge = 1.0e-6f;

    std::vector<std::uint32_t> filtered;
    filtered.reserve(mesh.Indices.size());
    std::size_t removed = 0;
    for (std::size_t ti = 0; ti < triCount; ++ti)
    {
        std::uint32_t i0 = mesh.Indices[ti * 3u + 0u];
        std::uint32_t i1 = mesh.Indices[ti * 3u + 1u];
        std::uint32_t i2 = mesh.Indices[ti * 3u + 2u];

        float l0 = edgeLength(i0, i1);
        float l1 = edgeLength(i1, i2);
        float l2 = edgeLength(i2, i0);
        if (!std::isfinite(l0) || !std::isfinite(l1) || !std::isfinite(l2))
        {
            ++removed;
            continue;
        }

        float minEdge = std::min(l0, std::min(l1, l2));
        float maxEdge = std::max(l0, std::max(l1, l2));
        if (maxEdge > longEdgeThreshold)
        {
            ++removed;
            continue;
        }
        if (minEdge < kMinEdge)
        {
            ++removed;
            continue;
        }
        if ((maxEdge / minEdge) > kAspectThreshold)
        {
            ++removed;
            continue;
        }

        filtered.push_back(i0);
        filtered.push_back(i1);
        filtered.push_back(i2);
    }

    if (filtered.size() >= 3u)
        mesh.Indices = std::move(filtered);
    else
        mesh.Indices.clear();
    return removed;
}

bool TryDecodePs3TrackMeshes(std::span<const std::uint8_t> cpuData,
                             std::span<const std::uint8_t> gpuData,
                             std::vector<MeshOutput>& outputs,
                             std::string_view preferredForestName = "track.forest",
                             bool decodeAllForests = false)
{
    outputs.clear();
    if (cpuData.size() < 0x20)
        return false;

    std::uint32_t forestCount = 0;
    if (!ReadBEU32(cpuData, 0, forestCount) || forestCount == 0 || forestCount > 512)
        return false;

    struct ForestEntry
    {
        std::string Name;
        std::uint32_t ForestOffset = 0;
    };

    std::vector<ForestEntry> entries;
    entries.reserve(forestCount);
    for (std::uint32_t i = 0; i < forestCount; ++i)
    {
        std::size_t entryOffset = 4u + static_cast<std::size_t>(i) * 0x10u;
        std::uint32_t namePtr = 0;
        std::uint32_t forestPtr = 0;
        if (!ReadBEU32(cpuData, entryOffset + 4, namePtr) ||
            !ReadBEU32(cpuData, entryOffset + 8, forestPtr))
        {
            continue;
        }
        if (forestPtr == 0 || static_cast<std::size_t>(forestPtr) >= cpuData.size())
            continue;

        entries.push_back({ReadCString(cpuData, namePtr), forestPtr});
    }

    if (entries.empty())
        return false;

    if (decodeAllForests)
    {
        bool any = false;
        for (auto const& e : entries)
        {
            std::vector<MeshOutput> local;
            if (TryDecodePs3TrackMeshes(cpuData, gpuData, local, e.Name, false))
            {
                outputs.insert(outputs.end(), local.begin(), local.end());
                any = true;
            }
        }
        return any;
    }

    std::vector<std::uint32_t> forestStarts;
    forestStarts.reserve(entries.size());
    for (auto const& entry : entries)
        forestStarts.push_back(entry.ForestOffset);
    std::sort(forestStarts.begin(), forestStarts.end());
    forestStarts.erase(std::unique(forestStarts.begin(), forestStarts.end()), forestStarts.end());

    auto findForestEnd = [&](std::uint32_t forestStart) -> std::size_t {
        std::size_t result = cpuData.size();
        for (std::uint32_t start : forestStarts)
        {
            if (start > forestStart)
            {
                result = static_cast<std::size_t>(start);
                break;
            }
        }
        return result;
    };

    auto nameMatch = [](std::string const& value, char const* wanted) {
        return ToLower(value) == wanted;
    };

    std::string preferredLower = ToLower(std::string(preferredForestName));
    ForestEntry target = entries.front();
    bool foundTarget = false;
    for (auto const& entry : entries)
    {
        if (nameMatch(entry.Name, preferredLower.c_str()))
        {
            target = entry;
            foundTarget = true;
            break;
        }
    }

    if (!foundTarget && preferredLower == "track.forest")
    {
        for (auto const& entry : entries)
        {
            if (nameMatch(entry.Name, "track_shadow.forest"))
            {
                target = entry;
                foundTarget = true;
                break;
            }
        }
    }

    if (!foundTarget && preferredLower != "track.forest")
        return false;

    std::size_t forestBase = static_cast<std::size_t>(target.ForestOffset);
    std::size_t forestEnd = findForestEnd(target.ForestOffset);
    if (forestEnd <= forestBase || forestEnd > cpuData.size())
        return false;

    std::size_t forestSpan = forestEnd - forestBase;
    auto inForest = [&](std::uint32_t relative, std::size_t length) -> bool {
        if (static_cast<std::size_t>(relative) > forestSpan)
            return false;
        return forestSpan - static_cast<std::size_t>(relative) >= length;
    };

    std::uint32_t treeCount = 0;
    std::uint32_t treeArray = 0;
    if (!ReadBEU32(cpuData, forestBase + 0x0, treeCount) ||
        !ReadBEU32(cpuData, forestBase + 0x4, treeArray))
    {
        return false;
    }
    if (treeCount == 0 || treeCount > 20000 || !inForest(treeArray, static_cast<std::size_t>(treeCount) * 4u))
        return false;

    Matrix4x4 identity = IdentityMatrix();
    constexpr float kBroadBound = 1.0e6f;
    std::size_t ps3SlotPrimitiveOutputs = 0;
    std::size_t ps3PackedPrimitiveOutputs = 0;
    std::size_t ps3PackedIndexedPrimitiveOutputs = 0;
    std::size_t ps3LegacyStreamOutputs = 0;
    std::size_t ps3PackedDebugDumped = 0;
    std::size_t ps3PackedExtra78Count = 0;
    std::size_t ps3PackedExtra80Count = 0;
    std::size_t ps3PackedExtra88Count = 0;
    std::size_t ps3PackedMultiCount = 0;
    std::size_t ps3PackedIndexedU8Count = 0;
    std::size_t ps3PackedIndexedU16Count = 0;
    std::size_t ps3PackedIndexedU32Count = 0;
    std::size_t ps3PackedEncodedMask3ffCount = 0;
    std::size_t ps3PackedEncodedMask0ffCount = 0;
    std::size_t ps3Packed10BitPrimitiveOutputs = 0;
    std::size_t ps3PackedIndirectPrimitiveOutputs = 0;
    std::size_t ps3PackedDropTokenTotal = 0;
    std::size_t ps3PackedDropRestartTotal = 0;
    std::size_t ps3PackedDropOutOfRangeTotal = 0;
    std::size_t ps3PackedDropRemapTotal = 0;
    std::size_t ps3PackedDropZeroIndexTotal = 0;
    std::size_t ps3PackedDropHeavyPrimitives = 0;
    std::size_t ps3StreamSlotReferenced = 0;
    std::size_t ps3StreamSlotResolved = 0;
    std::size_t ps3StreamSlotOob = 0;
    bool enableExperimentalU8Index = false;
    if (char const* envU8 = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_U8_INDEX"))
        enableExperimentalU8Index = (envU8[0] == '1');
    bool enableExperimentalEncodedMask = false;
    if (char const* envMask = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_ENCODED_MASK"))
        enableExperimentalEncodedMask = (envMask[0] == '1');
    bool enableExperimentalPackedPrimitiveIndex = false;
    if (char const* envPrimitive = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_PACKED_PRIMITIVE_INDEX"))
        enableExperimentalPackedPrimitiveIndex = (envPrimitive[0] == '1');
    bool logPackedIndexedPrimitive = false;
    if (char const* envLogIndexed = std::getenv("FOREST_TO_OBJ_PS3_LOG_PACKED_INDEXED"))
        logPackedIndexedPrimitive = (envLogIndexed[0] == '1');
    bool logPackedDroppedPrimitive = false;
    if (char const* envLogDropped = std::getenv("FOREST_TO_OBJ_PS3_LOG_DROPPED"))
        logPackedDroppedPrimitive = (envLogDropped[0] == '1');
    bool enableExperimentalFallbackRestartOnGap = false;
    if (char const* envRestartGap = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_FALLBACK_RESTART_ON_GAP"))
        enableExperimentalFallbackRestartOnGap = (envRestartGap[0] == '1');
    int experimentalFallbackMode = 4; // 0=strip, 1=list, 2=auto strip/list, 3=command-aware, 4=native-only (default)
    if (char const* envFallbackList = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_FALLBACK_LIST"))
    {
        if (envFallbackList[0] == '0')
            experimentalFallbackMode = 0;
        else if (envFallbackList[0] == '1')
            experimentalFallbackMode = 1;
        else if (envFallbackList[0] == '2')
            experimentalFallbackMode = 2;
        else if (envFallbackList[0] == '3')
            experimentalFallbackMode = 3;
        else if (envFallbackList[0] == '4')
            experimentalFallbackMode = 4;
    }
    float experimentalEncodedDominantMax = 0.08f;
    if (char const* envDom = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_ENCODED_DOM_MAX"))
    {
        float parsed = std::strtof(envDom, nullptr);
        if (std::isfinite(parsed))
            experimentalEncodedDominantMax = std::clamp(parsed, 0.0f, 1.0f);
    }
    float experimentalEncodedCoverageMin = 0.40f;
    if (char const* envCoverage = std::getenv("FOREST_TO_OBJ_PS3_EXPERIMENTAL_ENCODED_COVERAGE_MIN"))
    {
        float parsed = std::strtof(envCoverage, nullptr);
        if (std::isfinite(parsed))
            experimentalEncodedCoverageMin = std::clamp(parsed, 0.0f, 1.0f);
    }
    float experimentalDroppedWarnRatio = 0.35f;
    if (char const* envDropWarn = std::getenv("FOREST_TO_OBJ_PS3_DROPPED_WARN_RATIO"))
    {
        float parsed = std::strtof(envDropWarn, nullptr);
        if (std::isfinite(parsed))
            experimentalDroppedWarnRatio = std::clamp(parsed, 0.0f, 1.0f);
    }
    float experimentalCoverageWeight = 0.30f;
    bool logSlotDebug = false;
    int logSlotLimit = 8;
    if (char const* envSlot = std::getenv("FOREST_TO_OBJ_PS3_LOG_SLOT"))
    {
        logSlotDebug = (envSlot[0] == '1');
    }

    struct Ps3StreamTableChoice
    {
        std::vector<std::uint32_t> Entries;
        bool RelativeToForest = true;
        std::size_t TableBase = 0;
        int Score = 0;
    };

    auto parseStreamTableCandidate = [&](std::size_t tableBase, bool relativeToForest) -> Ps3StreamTableChoice {
        Ps3StreamTableChoice out;
        out.RelativeToForest = relativeToForest;
        out.TableBase = tableBase;
        if (tableBase + 8 > cpuData.size())
            return out;

        constexpr std::size_t kMaxEntries = 256;
        bool sawTerminator = false;
        for (std::size_t i = 0; i < kMaxEntries; ++i)
        {
            std::uint32_t entry = 0;
            if (!ReadBEU32(cpuData, tableBase + i * 4u, entry))
            {
                out.Entries.clear();
                return out;
            }
            if (entry == 0xFFFFFFFFu)
            {
                sawTerminator = true;
                break;
            }
            out.Entries.push_back(entry);
        }

        if (out.Entries.empty() || !sawTerminator)
            return out;

        std::size_t sample = std::min<std::size_t>(out.Entries.size(), 64u);
        for (std::size_t i = 0; i < sample; ++i)
        {
            std::size_t headerBase = relativeToForest
                ? forestBase + static_cast<std::size_t>(out.Entries[i])
                : static_cast<std::size_t>(out.Entries[i]);
            if (headerBase + 0x28 > cpuData.size())
                continue;

            std::uint32_t stride = 0;
            std::uint32_t count = 0;
            if (!ReadBEU32(cpuData, headerBase + 0x1C, stride) ||
                !ReadBEU32(cpuData, headerBase + 0x20, count))
            {
                continue;
            }
            if (stride >= 12u && stride <= 0x400u && count > 0u && count <= 5000000u)
                ++out.Score;
        }

        return out;
    };

    Ps3StreamTableChoice streamTableChoice;
    auto considerStreamTable = [&](std::size_t base, bool relativeToForest) {
        Ps3StreamTableChoice candidate = parseStreamTableCandidate(base, relativeToForest);
        if (candidate.Entries.empty())
            return;
        if (candidate.Score > streamTableChoice.Score ||
            (candidate.Score == streamTableChoice.Score && candidate.Entries.size() > streamTableChoice.Entries.size()))
        {
            streamTableChoice = std::move(candidate);
        }
    };

    if (forestBase + 0x144 <= cpuData.size())
    {
        considerStreamTable(forestBase + 0x144, true);
        considerStreamTable(forestBase + 0x144, false);
    }
    if (0x144u <= cpuData.size())
    {
        considerStreamTable(0x144u, true);
        considerStreamTable(0x144u, false);
    }

    if (streamTableChoice.Entries.empty())
    {
        std::size_t forestScanEnd = std::min<std::size_t>(forestEnd, forestBase + 0x1000u);
        for (std::size_t base = forestBase; base + 8 <= forestScanEnd; base += 4u)
        {
            considerStreamTable(base, true);
            considerStreamTable(base, false);
        }

        std::size_t globalScanEnd = std::min<std::size_t>(cpuData.size(), 0x2000u);
        for (std::size_t base = 0; base + 8 <= globalScanEnd; base += 4u)
        {
            considerStreamTable(base, true);
            considerStreamTable(base, false);
        }
    }

    struct Ps3RawPointer
    {
        bool Valid = false;
        bool IsGpu = false;
        std::uint32_t Offset = 0;
    };

    auto decodePs3RawPointer = [&](std::uint32_t raw) -> Ps3RawPointer {
        Ps3RawPointer out{};
        if (raw == 0)
            return out;

        std::uint32_t tag = (raw >> 24) & 0xFFu;
        std::uint32_t off24 = raw & 0x00FFFFFFu;
        if (tag == 0x1Au)
        {
            if (off24 < gpuData.size())
            {
                out.Valid = true;
                out.IsGpu = true;
                out.Offset = off24;
                return out;
            }
        }
        else if (tag == 0x08u || tag == 0x04u || tag == 0x00u)
        {
            if (off24 < cpuData.size())
            {
                out.Valid = true;
                out.IsGpu = false;
                out.Offset = off24;
                return out;
            }
        }

        if (raw < cpuData.size())
        {
            out.Valid = true;
            out.IsGpu = false;
            out.Offset = raw;
            return out;
        }

        constexpr std::uint32_t kCpuBase = 0x00200000u;
        constexpr std::uint32_t kGpuBase = 0x01800000u;
        if (raw >= kCpuBase)
        {
            std::uint32_t off = raw - kCpuBase;
            if (off < cpuData.size())
            {
                out.Valid = true;
                out.IsGpu = false;
                out.Offset = off;
                return out;
            }
        }
        if (raw >= kGpuBase)
        {
            std::uint32_t off = raw - kGpuBase;
            if (off < gpuData.size())
            {
                out.Valid = true;
                out.IsGpu = true;
                out.Offset = off;
                return out;
            }
        }

        return out;
    };

    for (std::uint32_t treeIndex = 0; treeIndex < treeCount; ++treeIndex)
    {
        std::uint32_t treeRel = 0;
        if (!ReadBEU32(cpuData, forestBase + static_cast<std::size_t>(treeArray) + static_cast<std::size_t>(treeIndex) * 4u, treeRel))
            continue;
        if (!inForest(treeRel, 0x1Cu))
            continue;

        std::size_t treeBase = forestBase + static_cast<std::size_t>(treeRel);
        std::uint32_t branchCount = 0;
        std::uint32_t branchArray = 0;
        std::uint32_t translationArray = 0;
        std::uint32_t rotationArray = 0;
        std::uint32_t scaleArray = 0;
        if (!ReadBEU32(cpuData, treeBase + 0x08, branchCount) ||
            !ReadBEU32(cpuData, treeBase + 0x0C, branchArray) ||
            !ReadBEU32(cpuData, treeBase + 0x10, translationArray) ||
            !ReadBEU32(cpuData, treeBase + 0x14, rotationArray) ||
            !ReadBEU32(cpuData, treeBase + 0x18, scaleArray))
        {
            continue;
        }

        if (branchCount == 0 || branchCount > 20000 || !inForest(branchArray, static_cast<std::size_t>(branchCount) * 4u))
            continue;

        std::vector<Vector4> translations(static_cast<std::size_t>(branchCount), {0.0f, 0.0f, 0.0f, 1.0f});
        std::vector<Vector4> rotations(static_cast<std::size_t>(branchCount), {0.0f, 0.0f, 0.0f, 1.0f});
        std::vector<Vector4> scales(static_cast<std::size_t>(branchCount), {1.0f, 1.0f, 1.0f, 1.0f});

        auto loadVec4Array = [&](std::uint32_t rel, std::vector<Vector4>& out) {
            if (!inForest(rel, out.size() * 16u))
                return;
            std::size_t base = forestBase + static_cast<std::size_t>(rel);
            for (std::size_t i = 0; i < out.size(); ++i)
            {
                Vector4 v{};
                if (ReadBEFloat4(cpuData, base + i * 16u, v))
                    out[i] = v;
            }
        };

        loadVec4Array(translationArray, translations);
        loadVec4Array(rotationArray, rotations);
        loadVec4Array(scaleArray, scales);

        struct BranchData
        {
            int Parent = -1;
            std::uint16_t Flags = 0;
            std::uint32_t Mesh = 0;
            std::uint32_t BranchRel = 0;
        };

        std::vector<BranchData> branches(static_cast<std::size_t>(branchCount));
        for (std::uint32_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
        {
            std::uint32_t branchRel = 0;
            if (!ReadBEU32(cpuData,
                           forestBase + static_cast<std::size_t>(branchArray) + static_cast<std::size_t>(branchIndex) * 4u,
                           branchRel))
            {
                continue;
            }
            if (!inForest(branchRel, 0x18u))
                continue;

            std::size_t branchBase = forestBase + static_cast<std::size_t>(branchRel);
            std::int16_t parent = -1;
            std::int16_t flags = 0;
            std::uint32_t mesh = 0;
            ReadBEI16(cpuData, branchBase + 0x0, parent);
            ReadBEI16(cpuData, branchBase + 0x6, flags);
            ReadBEU32(cpuData, branchBase + 0x14, mesh);

            branches[branchIndex].Parent = static_cast<int>(parent);
            branches[branchIndex].Flags = static_cast<std::uint16_t>(flags);
            branches[branchIndex].Mesh = mesh;
            branches[branchIndex].BranchRel = branchRel;
        }

        std::vector<Matrix4x4> worldMatrices(static_cast<std::size_t>(branchCount), identity);
        std::vector<std::uint8_t> worldState(static_cast<std::size_t>(branchCount), 0u);

        auto computeWorld = [&](auto&& self, int index) -> Matrix4x4 {
            if (index < 0 || static_cast<std::size_t>(index) >= worldMatrices.size())
                return identity;

            std::size_t idx = static_cast<std::size_t>(index);
            if (worldState[idx] == 2u)
                return worldMatrices[idx];
            if (worldState[idx] == 1u)
                return identity;

            worldState[idx] = 1u;

            Vector4 t = translations[idx];
            Vector4 r = NormalizeQuaternion(rotations[idx]);
            Vector4 s = scales[idx];

            if (!std::isfinite(t.X) || std::abs(t.X) > 50000.0f)
                t.X = 0.0f;
            if (!std::isfinite(t.Y) || std::abs(t.Y) > 50000.0f)
                t.Y = 0.0f;
            if (!std::isfinite(t.Z) || std::abs(t.Z) > 50000.0f)
                t.Z = 0.0f;
            t.W = 1.0f;

            s.X = SanitizeScale(s.X);
            s.Y = SanitizeScale(s.Y);
            s.Z = SanitizeScale(s.Z);
            s.W = 1.0f;

            Matrix4x4 local = BuildLocalMatrix(t, r, s);
            int parent = branches[idx].Parent;
            if (parent >= 0 && static_cast<std::size_t>(parent) < worldMatrices.size() && parent != index)
                worldMatrices[idx] = Multiply(self(self, parent), local);
            else
                worldMatrices[idx] = local;

            worldState[idx] = 2u;
            return worldMatrices[idx];
        };

        for (std::uint32_t branchIndex = 0; branchIndex < branchCount; ++branchIndex)
        {
            auto const& branch = branches[branchIndex];
            Matrix4x4 worldMatrix = computeWorld(computeWorld, static_cast<int>(branchIndex));

            std::vector<std::uint32_t> meshCandidates;
            if ((branch.Flags & 8u) != 0u && inForest(branch.Mesh, 0x18u))
                meshCandidates.push_back(branch.Mesh);

            // LOD branches inline SuLodBranch data directly after SuBranch base (+0x14).
            // Walk all thresholds so we don't silently drop LOD-only geometry.
            if ((branch.Flags & 16u) != 0u)
            {
                std::uint32_t lodRel = branch.BranchRel + 0x14u;
                if (inForest(lodRel, 0x5Cu))
                {
                    std::size_t lodBase = forestBase + static_cast<std::size_t>(lodRel);
                    std::uint32_t thresholdCount = 0;
                    std::uint32_t thresholdArray = 0;
                    if (ReadBEU32(cpuData, lodBase + 0x0, thresholdCount) &&
                        ReadBEU32(cpuData, lodBase + 0x4, thresholdArray) &&
                        thresholdCount > 0 && thresholdCount <= 256 &&
                        inForest(thresholdArray, static_cast<std::size_t>(thresholdCount) * 0xCu))
                    {
                        for (std::uint32_t ti = 0; ti < thresholdCount; ++ti)
                        {
                            std::size_t thresholdBase = forestBase + static_cast<std::size_t>(thresholdArray) +
                                static_cast<std::size_t>(ti) * 0xCu;
                            std::uint32_t thresholdMesh = 0;
                            if (!ReadBEU32(cpuData, thresholdBase + 0x4, thresholdMesh))
                                continue;
                            if (!inForest(thresholdMesh, 0x18u))
                                continue;
                            if (std::find(meshCandidates.begin(), meshCandidates.end(), thresholdMesh) == meshCandidates.end())
                                meshCandidates.push_back(thresholdMesh);
                        }
                    }
                }
            }

            for (std::uint32_t meshRel : meshCandidates)
            {
                if (!inForest(meshRel, 0x18u))
                    continue;

                std::size_t meshBase = forestBase + static_cast<std::size_t>(meshRel);

                std::uint32_t primitiveCount = 0;
                std::uint32_t primitiveArray = 0;
                if (!ReadBEU32(cpuData, meshBase + 0x10, primitiveCount) ||
                    !ReadBEU32(cpuData, meshBase + 0x14, primitiveArray))
                {
                    continue;
                }
                if (primitiveCount == 0 || primitiveCount > 10000 ||
                    !inForest(primitiveArray, static_cast<std::size_t>(primitiveCount) * 4u))
                {
                    continue;
                }

                for (std::uint32_t primitiveIndex = 0; primitiveIndex < primitiveCount; ++primitiveIndex)
                {
                    std::uint32_t primitiveRel = 0;
                    if (!ReadBEU32(cpuData,
                                   forestBase + static_cast<std::size_t>(primitiveArray) + static_cast<std::size_t>(primitiveIndex) * 4u,
                                   primitiveRel))
                    {
                        continue;
                    }
                    if (!inForest(primitiveRel, 0xA8u))
                        continue;

                    std::size_t primitiveBase = forestBase + static_cast<std::size_t>(primitiveRel);

                    bool emitted = false;

                    // Preferred PS3 track path: slot-table stream headers + explicit index buffer.
                    if (!streamTableChoice.Entries.empty())
                    {
                        std::uint32_t slot32 = 0;
                        if (ReadBEU32(cpuData, primitiveBase + 0x9C, slot32))
                        {
                            std::uint8_t slot8 = cpuData[primitiveBase + 0x9F];
                            std::uint32_t streamSlot = (slot32 <= 0xFFu)
                                ? slot32
                                : static_cast<std::uint32_t>(slot8);

                            if (streamSlot < streamTableChoice.Entries.size())
                            {
                                ++ps3StreamSlotReferenced;
                                std::size_t streamHeaderBase = streamTableChoice.RelativeToForest
                                    ? forestBase + static_cast<std::size_t>(streamTableChoice.Entries[streamSlot])
                                    : static_cast<std::size_t>(streamTableChoice.Entries[streamSlot]);

                                // PS3 stream records (observed in OpaOpa):
                                // 0x00 sigOffset (BE, into .sig), 0x04 desc, 0x08 slotKey,
                                // 0x0C choiceDup, 0x10 unk, 0x14 zero, 0x18 zero, 0x1C byteLen.
                                if (streamHeaderBase + 0x20 <= cpuData.size())
                                {
                                    std::uint32_t sigOffset = 0;
                                    std::uint32_t byteLen = 0;
                                    if (ReadBEU32(cpuData, streamHeaderBase + 0x0, sigOffset) &&
                                        ReadBEU32(cpuData, streamHeaderBase + 0x1C, byteLen) &&
                                        sigOffset < gpuData.size() &&
                                        static_cast<std::uint64_t>(sigOffset) + byteLen <= gpuData.size() &&
                                        byteLen >= 12u)
                                    {
                                        auto scoreVb = [&](std::uint32_t stride, std::uint32_t vtxCount) -> int {
                                            std::uint64_t bytes = static_cast<std::uint64_t>(stride) * static_cast<std::uint64_t>(vtxCount);
                                            if (bytes == 0 || bytes > byteLen)
                                                return -1;
                                            std::uint32_t sampleCount = std::min<std::uint32_t>(vtxCount, 64u);
                                            int score = 0;
                                            int scoreLe = 0;
                                            for (std::uint32_t vi = 0; vi < sampleCount; ++vi)
                                            {
                                                std::size_t posOffset = static_cast<std::size_t>(sigOffset) +
                                                    static_cast<std::size_t>(vi) * static_cast<std::size_t>(stride);
                                                Vector3 p{};
                                                if (!ReadBEFloat3(gpuData, posOffset, p))
                                                    continue;
                                                float maxAbs = std::max(std::abs(p.X), std::max(std::abs(p.Y), std::abs(p.Z)));
                                                if (std::isfinite(maxAbs) && maxAbs < kBroadBound)
                                                    ++score;
                                                if (gpuData.size() >= posOffset + 12u)
                                                {
                                                    float lx = 0.0f, ly = 0.0f, lz = 0.0f;
                                                    std::memcpy(&lx, gpuData.data() + posOffset + 0, 4);
                                                    std::memcpy(&ly, gpuData.data() + posOffset + 4, 4);
                                                    std::memcpy(&lz, gpuData.data() + posOffset + 8, 4);
                                                    float lmax = std::max(std::abs(lx), std::max(std::abs(ly), std::abs(lz)));
                                                    if (std::isfinite(lmax) && lmax < kBroadBound)
                                                        ++scoreLe;
                                                }
                                            }
                                            return std::max(score, scoreLe);
                                        };

                                        struct VbCandidate
                                        {
                                            std::uint32_t Start = 0;
                                            std::uint32_t Stride = 0;
                                            std::uint32_t VertexCount = 0;
                                            int Score = -1;
                                        };
                                        VbCandidate bestVb{};

                                        constexpr std::array<std::uint32_t, 9> kStrideTry{12, 16, 20, 24, 28, 32, 36, 40, 48};
                                        for (std::uint32_t stride : kStrideTry)
                                        {
                                            if (stride == 0)
                                                continue;
                                            if (byteLen % stride != 0)
                                                continue;
                                            std::uint32_t vtxCount = byteLen / stride;
                                            if (vtxCount < 3u || vtxCount > 2000000u)
                                                continue;
                                            int s = scoreVb(stride, vtxCount);
                                            if (s > bestVb.Score)
                                            {
                                                bestVb = {sigOffset, stride, vtxCount, s};
                                            }
                                        }

                                        if (logSlotDebug && logSlotLimit > 0 && bestVb.Score <= 0)
                                        {
                                            --logSlotLimit;
                                            std::cerr << "[PS3] slot miss " << static_cast<int>(streamSlot)
                                                      << " hdr=0x" << std::hex << streamHeaderBase
                                                      << " len=0x" << byteLen
                                                      << " sigOff=0x" << sigOffset
                                                      << std::dec << '\n';
                                        }

                                        if (bestVb.Score > 0)
                                        {
                                            if (logSlotDebug && logSlotLimit > 0)
                                            {
                                                --logSlotLimit;
                                                std::cerr << "[PS3] slot " << static_cast<int>(streamSlot)
                                                          << " hdr=0x" << std::hex << streamHeaderBase
                                                          << " stride=" << std::dec << bestVb.Stride
                                                          << " vtx=" << bestVb.VertexCount
                                                          << " score=" << bestVb.Score
                                                          << " start=0x" << std::hex << bestVb.Start
                                                          << std::dec << '\n';
                                            }
                                            ++ps3StreamSlotResolved;
                                            struct IndexLayout
                                            {
                                                std::size_t NumIndicesField = 0;
                                                std::size_t IndexPtrField = 0;
                                            };
                                            std::array<IndexLayout, 2> indexLayouts{{
                                                {0x90u, 0x94u}, // track-specific primitive layout
                                                {0x8Cu, 0x90u}  // standard SuRenderPrimitive layout
                                            }};

                                            for (auto const& layout : indexLayouts)
                                            {
                                                std::uint32_t numIndices = 0;
                                                std::uint32_t indexPtrRaw = 0;
                                                if (!ReadBEU32(cpuData, primitiveBase + layout.NumIndicesField, numIndices) ||
                                                    !ReadBEU32(cpuData, primitiveBase + layout.IndexPtrField, indexPtrRaw))
                                                {
                                                    continue;
                                                }
                                                if (numIndices < 3u || numIndices > 2000000u)
                                                    continue;

                                                Ps3RawPointer indexPtr = decodePs3RawPointer(indexPtrRaw);
                                                if (!indexPtr.Valid)
                                                    continue;

                                                std::span<const std::uint8_t> indexData = indexPtr.IsGpu ? gpuData : cpuData;
                                                if (indexPtr.Offset >= indexData.size())
                                                    continue;

                                                auto canReadIndex = [&](std::size_t bytesPerIndex) -> bool {
                                                    std::uint64_t bytes = static_cast<std::uint64_t>(numIndices) * bytesPerIndex;
                                                    return static_cast<std::uint64_t>(indexPtr.Offset) + bytes <= indexData.size();
                                                };
                                                auto readIndexU8 = [&](std::size_t off) -> std::uint32_t {
                                                    if (off + 1 > indexData.size())
                                                        return 0;
                                                    return static_cast<std::uint32_t>(indexData[off]);
                                                };
                                                auto readIndexU16 = [&](std::size_t off) -> std::uint16_t {
                                                    if (off + 2 > indexData.size())
                                                        return 0;
                                                    return static_cast<std::uint16_t>((indexData[off] << 8) | indexData[off + 1]);
                                                };
                                                auto readIndexU32 = [&](std::size_t off) -> std::uint32_t {
                                                    if (off + 4 > indexData.size())
                                                        return 0;
                                                    return (static_cast<std::uint32_t>(indexData[off]) << 24) |
                                                           (static_cast<std::uint32_t>(indexData[off + 1]) << 16) |
                                                           (static_cast<std::uint32_t>(indexData[off + 2]) << 8) |
                                                           static_cast<std::uint32_t>(indexData[off + 3]);
                                                };
                                                auto scoreIndexFormat = [&](std::size_t bytesPerIndex) -> int {
                                                    if (!canReadIndex(bytesPerIndex))
                                                        return -1;
                                                    std::size_t sample = std::min<std::size_t>(numIndices, 512u);
                                                    int ok = 0;
                                                    for (std::size_t ii = 0; ii < sample; ++ii)
                                                    {
                                                        std::size_t off = static_cast<std::size_t>(indexPtr.Offset) + ii * bytesPerIndex;
                                                        std::uint32_t idx = bytesPerIndex == 1 ? readIndexU8(off)
                                                            : (bytesPerIndex == 2 ? readIndexU16(off) : readIndexU32(off));
                                                        bool restart = (bytesPerIndex == 1 && (idx == 0x00u || idx == 0xFFu)) ||
                                                                       (bytesPerIndex == 2 && idx == 0xFFFFu) ||
                                                                       (bytesPerIndex == 4 && idx == 0xFFFFFFFFu);
                                                        if (restart || idx >= bestVb.VertexCount)
                                                            continue;
                                                        ++ok;
                                                    }
                                                    return ok;
                                                };

                                                int score8 = enableExperimentalU8Index ? scoreIndexFormat(1) : -1;
                                                int score16 = scoreIndexFormat(2);
                                                int score32 = scoreIndexFormat(4);
                                                std::size_t bytesPerIndex = score32 > score16 ? 4u : 2u;
                                                int bestScore = std::max(score16, score32);
                                                if (score8 > bestScore)
                                                {
                                                    bestScore = score8;
                                                    bytesPerIndex = 1u;
                                                }
                                                if (!canReadIndex(bytesPerIndex))
                                                    continue;

                                                std::span<const std::uint8_t> vertexData = gpuData;
                                                std::vector<int> remap(static_cast<std::size_t>(bestVb.VertexCount), -1);
                                                std::vector<ObjVertex> vertices;
                                                vertices.reserve(static_cast<std::size_t>(bestVb.VertexCount));

                                                for (std::uint32_t vi = 0; vi < bestVb.VertexCount; ++vi)
                                                {
                                                    std::size_t posOffset = static_cast<std::size_t>(bestVb.Start) +
                                                        static_cast<std::size_t>(vi) * static_cast<std::size_t>(bestVb.Stride);

                                                    Vector3 localPos{};
                                                    if (!ReadBEFloat3(vertexData, posOffset, localPos))
                                                        continue;

                                                    Vector4 pos4{localPos.X, localPos.Y, localPos.Z, 1.0f};
                                                    Vector4 transformed = Transform(worldMatrix, pos4);
                                                    if (!std::isfinite(transformed.X) || !std::isfinite(transformed.Y) || !std::isfinite(transformed.Z))
                                                        continue;

                                                    ObjVertex v{};
                                                    v.Pos = {transformed.X, transformed.Y, transformed.Z};
                                                    v.Normal = {0.0f, 1.0f, 0.0f};
                                                    v.Uv = DecodePs3UvTail(vertexData,
                                                                           static_cast<std::size_t>(bestVb.Start),
                                                                           vi,
                                                                           static_cast<int>(bestVb.Stride));

                                                    remap[static_cast<std::size_t>(vi)] = static_cast<int>(vertices.size());
                                                    vertices.push_back(v);
                                                }

                                                if (vertices.size() < 3)
                                                    continue;

                                                std::vector<std::uint32_t> indices;
                                                indices.reserve(static_cast<std::size_t>(numIndices) * 3u);

                                                if (bytesPerIndex == 1u)
                                                {
                                                    for (std::uint32_t ii = 0; ii < numIndices; ++ii)
                                                    {
                                                        std::size_t off = static_cast<std::size_t>(indexPtr.Offset) +
                                                            static_cast<std::size_t>(ii);
                                                        std::uint32_t idx = readIndexU8(off);
                                                        bool restart = (idx == 0x00u || idx == 0xFFu);
                                                        if (restart || idx >= bestVb.VertexCount)
                                                            continue;
                                                        int mapped = remap[static_cast<std::size_t>(idx)];
                                                        if (mapped < 0)
                                                            continue;
                                                        indices.push_back(static_cast<std::uint32_t>(mapped));
                                                    }
                                                    if (indices.size() < 3u)
                                                        continue;
                                                    if (indices.size() % 3u != 0u)
                                                        indices.resize(indices.size() - (indices.size() % 3u));
                                                }
                                                else
                                                {
                                                    bool have0 = false;
                                                    bool have1 = false;
                                                    bool flip = false;
                                                    std::uint32_t i0 = 0;
                                                    std::uint32_t i1 = 0;
                                                    for (std::uint32_t ii = 0; ii < numIndices; ++ii)
                                                    {
                                                        std::size_t off = static_cast<std::size_t>(indexPtr.Offset) +
                                                            static_cast<std::size_t>(ii) * bytesPerIndex;
                                                        std::uint32_t idx = bytesPerIndex == 1 ? readIndexU8(off)
                                                            : (bytesPerIndex == 2 ? readIndexU16(off) : readIndexU32(off));
                                                        bool restart = (bytesPerIndex == 1 && (idx == 0x00u || idx == 0xFFu)) ||
                                                                       (bytesPerIndex == 2 && idx == 0xFFFFu) ||
                                                                       (bytesPerIndex == 4 && idx == 0xFFFFFFFFu);
                                                        if (restart || idx >= bestVb.VertexCount)
                                                        {
                                                            have0 = false;
                                                            have1 = false;
                                                            flip = false;
                                                            continue;
                                                        }

                                                        int mapped = remap[static_cast<std::size_t>(idx)];
                                                        if (mapped < 0)
                                                        {
                                                            have0 = false;
                                                            have1 = false;
                                                            flip = false;
                                                            continue;
                                                        }

                                                        std::uint32_t m = static_cast<std::uint32_t>(mapped);
                                                        if (!have0)
                                                        {
                                                            i0 = m;
                                                            have0 = true;
                                                            continue;
                                                        }
                                                        if (!have1)
                                                        {
                                                            i1 = m;
                                                            have1 = true;
                                                            continue;
                                                        }

                                                        if (i0 != i1 && i1 != m && i0 != m)
                                                        {
                                                            if (flip)
                                                            {
                                                                indices.push_back(i1);
                                                                indices.push_back(i0);
                                                                indices.push_back(m);
                                                            }
                                                            else
                                                            {
                                                                indices.push_back(i0);
                                                                indices.push_back(i1);
                                                                indices.push_back(m);
                                                            }
                                                        }
                                                        i0 = i1;
                                                        i1 = m;
                                                        flip = !flip;
                                                    }
                                                }

                                                if (indices.size() < 3)
                                                    continue;

                                                MeshOutput out;
                                                out.Name = target.Name + "_t" + std::to_string(treeIndex) +
                                                    "_b" + std::to_string(branchIndex) +
                                                    "_m" + std::to_string(meshRel) +
                                                    "_p" + std::to_string(primitiveIndex);
                                                out.Vertices = std::move(vertices);
                                                out.Indices = std::move(indices);
                                                outputs.emplace_back(std::move(out));
                                                emitted = true;
                                                ++ps3SlotPrimitiveOutputs;
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            ++ps3StreamSlotOob;
                                        }
                                    }
                                    else
                                    {
                                        ++ps3StreamSlotOob;
                                    }
                                }
                            }
                        }
                    }

                    if (emitted)
                        continue;


                // Fallback PS3 path (kept for resilience): infer strips directly from packed stream blobs.
                //   +0x70 : position stream byte size
                //   +0x74 : position stream pointer (BE float3 array)
                // This path is useful when slot-table/index pointers are incomplete.
                std::uint32_t packedStreamRel = 0;
                if (ReadBEU32(cpuData, primitiveBase + 0x90, packedStreamRel) && inForest(packedStreamRel, 0xA8u))
                {
                    std::size_t packedStreamBase = forestBase + static_cast<std::size_t>(packedStreamRel);
                    std::uint32_t positionBytes = 0;
                    std::uint32_t positionData = 0;
                    if (ReadBEU32(cpuData, packedStreamBase + 0x70, positionBytes) &&
                        ReadBEU32(cpuData, packedStreamBase + 0x74, positionData) &&
                        positionBytes >= 36u &&
                        positionBytes <= 0x200000u &&
                        inForest(positionData, static_cast<std::size_t>(positionBytes)))
                    {
                        if (false && ps3PackedDebugDumped < 24)
                        {
                            std::cerr << "[PS3] packed primitive rel=0x" << std::hex << primitiveRel
                                      << " pack=0x" << packedStreamRel
                                      << " posBytes=0x" << positionBytes
                                      << " posData=0x" << positionData
                                      << std::dec << '\n';
                            for (std::size_t off = 0x80; off <= 0xA4; off += 4)
                            {
                                std::uint32_t v = 0;
                                ReadBEU32(cpuData, primitiveBase + off, v);
                                std::cerr << "  prim[+" << std::hex << off << "] 0x" << v << std::dec << '\n';
                            }
                            for (std::size_t off = 0x60; off <= 0xA4; off += 4)
                            {
                                std::uint32_t v = 0;
                                ReadBEU32(cpuData, packedStreamBase + off, v);
                                std::cerr << "  pack[+" << std::hex << off << "] 0x" << v << std::dec << '\n';
                            }
                            std::uint32_t streamRelDbg = 0;
                            if (ReadBEU32(cpuData, primitiveBase + 0x98, streamRelDbg) && inForest(streamRelDbg, 0x40u))
                            {
                                std::size_t streamBaseDbg = forestBase + static_cast<std::size_t>(streamRelDbg);
                                std::cerr << "  streamRel=0x" << std::hex << streamRelDbg << std::dec << '\n';
                                for (std::size_t off = 0x0; off <= 0x40; off += 4)
                                {
                                    std::uint32_t v = 0;
                                    ReadBEU32(cpuData, streamBaseDbg + off, v);
                                    std::cerr << "  stream[+" << std::hex << off << "] 0x" << v << std::dec << '\n';
                                }

                                std::uint32_t streamField8 = 0;
                                std::uint32_t primitiveField88 = 0;
                                ReadBEU32(cpuData, streamBaseDbg + 0x8, streamField8);
                                ReadBEU32(cpuData, primitiveBase + 0x88, primitiveField88);
                                std::uint32_t dbgVertexCount = positionBytes / 12u;
                                if (primitiveField88 > streamField8 &&
                                    inForest(streamField8, static_cast<std::size_t>(primitiveField88 - streamField8)))
                                {
                                    std::size_t sampleU16 = std::min<std::size_t>(
                                        static_cast<std::size_t>((primitiveField88 - streamField8) / 2u), 512u);
                                    std::size_t okU16 = 0;
                                    std::size_t restartU16 = 0;
                                    for (std::size_t si = 0; si < sampleU16; ++si)
                                    {
                                        std::uint16_t idx = 0;
                                        if (!ReadBEU16(cpuData,
                                                       forestBase + static_cast<std::size_t>(streamField8) + si * 2u,
                                                       idx))
                                        {
                                            break;
                                        }
                                        if (idx == 0xFFFFu)
                                        {
                                            ++restartU16;
                                            continue;
                                        }
                                        if (idx < dbgVertexCount)
                                            ++okU16;
                                    }
                                    std::cerr << "  idxProbe start=0x" << std::hex << streamField8
                                              << " end=0x" << primitiveField88 << std::dec
                                              << " bytes=" << (primitiveField88 - streamField8)
                                              << " sample=" << sampleU16
                                              << " okU16=" << okU16
                                              << " restartU16=" << restartU16
                                              << " vertexCount=" << dbgVertexCount
                                              << '\n';
                                }
                            }
                            ++ps3PackedDebugDumped;
                        }

                        auto hasValidPackedPair = [&](std::size_t bytesOff, std::size_t dataOff) {
                            std::uint32_t bytes = 0;
                            std::uint32_t data = 0;
                            return ReadBEU32(cpuData, packedStreamBase + bytesOff, bytes) &&
                                   ReadBEU32(cpuData, packedStreamBase + dataOff, data) &&
                                   bytes >= 36u && bytes <= 0x200000u &&
                                   inForest(data, static_cast<std::size_t>(bytes));
                        };
                        if (hasValidPackedPair(0x78u, 0x7Cu))
                            ++ps3PackedExtra78Count;
                        if (hasValidPackedPair(0x80u, 0x84u))
                            ++ps3PackedExtra80Count;
                        if (hasValidPackedPair(0x88u, 0x8Cu))
                            ++ps3PackedExtra88Count;
                        std::uint32_t packedCountField = 0;
                        if (ReadBEU32(cpuData, primitiveBase + 0x8C, packedCountField) && packedCountField > 1u)
                            ++ps3PackedMultiCount;

                        std::uint32_t primaryVertexCount = positionBytes / 12u;
                        std::uint32_t extraPositionBytes = 0;
                        std::uint32_t extraPositionData = 0;
                        bool hasExtraPositionStream =
                            ReadBEU32(cpuData, packedStreamBase + 0x78, extraPositionBytes) &&
                            ReadBEU32(cpuData, packedStreamBase + 0x7C, extraPositionData) &&
                            extraPositionBytes >= 36u &&
                            extraPositionBytes <= 0x200000u &&
                            inForest(extraPositionData, static_cast<std::size_t>(extraPositionBytes));

                        std::uint32_t extraVertexCount = hasExtraPositionStream ? (extraPositionBytes / 12u) : 0u;
                        std::uint32_t vertexCount = primaryVertexCount + extraVertexCount;
                        std::vector<std::pair<std::uint32_t, std::uint32_t>> fallbackRanges;
                        fallbackRanges.reserve(2u);
                        if (primaryVertexCount >= 3u)
                            fallbackRanges.emplace_back(0u, primaryVertexCount);
                        if (hasExtraPositionStream && extraVertexCount >= 3u)
                            fallbackRanges.emplace_back(primaryVertexCount, primaryVertexCount + extraVertexCount);
                        if (vertexCount >= 3u)
                        {
                            std::vector<int> remap(static_cast<std::size_t>(vertexCount), -1);
                            std::vector<ObjVertex> vertices;
                            vertices.reserve(static_cast<std::size_t>(vertexCount));

                            auto appendPackedVertices = [&](std::uint32_t dataRel,
                                                            std::uint32_t streamVertexCount,
                                                            std::uint32_t remapBase) {
                                for (std::uint32_t vi = 0; vi < streamVertexCount; ++vi)
                                {
                                    std::size_t posOffset = forestBase + static_cast<std::size_t>(dataRel) +
                                        static_cast<std::size_t>(vi) * 12u;

                                    Vector3 localPos{};
                                    if (!ReadBEFloat3(cpuData, posOffset, localPos))
                                        continue;

                                    Vector4 pos4{localPos.X, localPos.Y, localPos.Z, 1.0f};
                                    Vector4 transformed = Transform(worldMatrix, pos4);
                                    if (!std::isfinite(transformed.X) || !std::isfinite(transformed.Y) || !std::isfinite(transformed.Z))
                                        continue;

                                    ObjVertex v{};
                                    v.Pos = {transformed.X, transformed.Y, transformed.Z};
                                    v.Normal = {0.0f, 1.0f, 0.0f};
                                    v.Uv = {};

                                    std::size_t remapIndex = static_cast<std::size_t>(remapBase) + static_cast<std::size_t>(vi);
                                    if (remapIndex >= remap.size())
                                        continue;
                                    remap[remapIndex] = static_cast<int>(vertices.size());
                                    vertices.push_back(v);
                                }
                            };

                            appendPackedVertices(positionData, primaryVertexCount, 0u);
                            if (hasExtraPositionStream && extraVertexCount > 0u)
                                appendPackedVertices(extraPositionData, extraVertexCount, primaryVertexCount);

                            if (vertices.size() >= 3)
                            {
                                struct Ps3DecodeDropStats
                                {
                                    std::size_t TokenTotal = 0;
                                    std::size_t Restart = 0;
                                    std::size_t OutOfRange = 0;
                                    std::size_t RemapDrop = 0;
                                    std::size_t ZeroIndex = 0;
                                };

                                struct IndexedDecodeResult
                                {
                                    std::vector<std::uint32_t> Indices;
                                    std::vector<std::uint32_t> ListIndices;
                                    int Score = -1;
                                    int SampleCount = 0;
                                    std::size_t NumField = 0;
                                    std::size_t PtrField = 0;
                                    std::size_t BytesPerIndex = 0;
                                    std::uint32_t NumIndices = 0;
                                    std::uint32_t IndexPtrRaw = 0;
                                    Ps3DecodeDropStats Drops{};
                                };

                                struct StripDecodeResult
                                {
                                    std::vector<std::uint32_t> Indices;
                                    Ps3DecodeDropStats Drops{};
                                };

                                enum class Packed10IndexStyle
                                {
                                    Direct,
                                    DirectZeroRestart,
                                    OneBasedZeroRestart
                                };

                                enum class Packed10PrimitiveMode
                                {
                                    Strip,
                                    List
                                };

                                auto decodeIndexedStrip = [&](std::size_t numIndicesField,
                                                              std::size_t indexPtrField) -> IndexedDecodeResult {
                                    IndexedDecodeResult result;
                                    result.NumField = numIndicesField;
                                    result.PtrField = indexPtrField;

                                    std::vector<std::uint32_t> out;
                                    std::uint32_t numIndices = 0;
                                    std::uint32_t indexPtrRaw = 0;
                                    if (!ReadBEU32(cpuData, primitiveBase + numIndicesField, numIndices) ||
                                        !ReadBEU32(cpuData, primitiveBase + indexPtrField, indexPtrRaw) ||
                                        numIndices < 3u || numIndices > 2000000u)
                                    {
                                        return result;
                                    }
                                    result.NumIndices = numIndices;
                                    result.IndexPtrRaw = indexPtrRaw;

                                    Ps3RawPointer indexPtr = decodePs3RawPointer(indexPtrRaw);
                                    if (!indexPtr.Valid)
                                        return result;

                                    std::span<const std::uint8_t> indexData = indexPtr.IsGpu ? gpuData : cpuData;
                                    if (indexPtr.Offset >= indexData.size())
                                        return result;

                                    auto canReadIndex = [&](std::size_t bytesPerIndex) -> bool {
                                        std::uint64_t bytes = static_cast<std::uint64_t>(numIndices) * bytesPerIndex;
                                        return static_cast<std::uint64_t>(indexPtr.Offset) + bytes <= indexData.size();
                                    };
                                    auto readIndexU8 = [&](std::size_t off) -> std::uint32_t {
                                        if (off + 1 > indexData.size())
                                            return 0;
                                        return static_cast<std::uint32_t>(indexData[off]);
                                    };
                                    auto readIndexU16 = [&](std::size_t off) -> std::uint16_t {
                                        if (off + 2 > indexData.size())
                                            return 0;
                                        return static_cast<std::uint16_t>((indexData[off] << 8) | indexData[off + 1]);
                                    };
                                    auto readIndexU32 = [&](std::size_t off) -> std::uint32_t {
                                        if (off + 4 > indexData.size())
                                            return 0;
                                        return (static_cast<std::uint32_t>(indexData[off]) << 24) |
                                               (static_cast<std::uint32_t>(indexData[off + 1]) << 16) |
                                               (static_cast<std::uint32_t>(indexData[off + 2]) << 8) |
                                               static_cast<std::uint32_t>(indexData[off + 3]);
                                    };
                                    auto scoreIndexFormat = [&](std::size_t bytesPerIndex) -> int {
                                        if (!canReadIndex(bytesPerIndex))
                                            return -1;
                                        std::size_t sample = std::min<std::size_t>(numIndices, 512u);
                                        int ok = 0;
                                        for (std::size_t ii = 0; ii < sample; ++ii)
                                        {
                                            std::size_t off = static_cast<std::size_t>(indexPtr.Offset) + ii * bytesPerIndex;
                                            std::uint32_t idx = bytesPerIndex == 1 ? readIndexU8(off)
                                                : (bytesPerIndex == 2 ? readIndexU16(off) : readIndexU32(off));
                                            if ((bytesPerIndex == 1 && (idx == 0x00u || idx == 0xFFu)) ||
                                                (bytesPerIndex == 2 && idx == 0xFFFFu) ||
                                                (bytesPerIndex == 4 && idx == 0xFFFFFFFFu))
                                            {
                                                continue;
                                            }
                                            if (idx < vertexCount)
                                                ++ok;
                                        }
                                        return ok;
                                    };

                                    int score8 = enableExperimentalU8Index ? scoreIndexFormat(1) : -1;
                                    int score16 = scoreIndexFormat(2);
                                    int score32 = scoreIndexFormat(4);
                                    std::size_t bytesPerIndex = score32 > score16 ? 4u : 2u;
                                    int bestScore = std::max(score16, score32);
                                    if (score8 > bestScore)
                                    {
                                        bestScore = score8;
                                        bytesPerIndex = 1u;
                                    }
                                    if (!canReadIndex(bytesPerIndex))
                                        return result;

                                    std::size_t sampleCount = std::min<std::size_t>(numIndices, 512u);
                                    if (sampleCount == 0 || bestScore * 5 < static_cast<int>(sampleCount) * 4)
                                        return result;

                                    out.reserve(static_cast<std::size_t>(numIndices) * 3u);
                                    if (bytesPerIndex == 1u)
                                    {
                                        for (std::uint32_t ii = 0; ii < numIndices; ++ii)
                                        {
                                            ++result.Drops.TokenTotal;
                                            std::size_t off = static_cast<std::size_t>(indexPtr.Offset) +
                                                static_cast<std::size_t>(ii);
                                            std::uint32_t idx = readIndexU8(off);
                                            if (idx == 0u)
                                                ++result.Drops.ZeroIndex;
                                            bool restart = (idx == 0x00u || idx == 0xFFu);
                                            if (restart || idx >= vertexCount)
                                            {
                                                if (restart)
                                                    ++result.Drops.Restart;
                                                else
                                                    ++result.Drops.OutOfRange;
                                                continue;
                                            }

                                            int mapped = remap[static_cast<std::size_t>(idx)];
                                            if (mapped < 0)
                                            {
                                                ++result.Drops.RemapDrop;
                                                continue;
                                            }
                                            out.push_back(static_cast<std::uint32_t>(mapped));
                                        }
                                        if (out.size() % 3u != 0u)
                                            out.resize(out.size() - (out.size() % 3u));
                                        result.ListIndices = out;
                                    }
                                    else
                                    {
                                        std::vector<std::uint32_t> listOut;
                                        listOut.reserve(static_cast<std::size_t>(numIndices));
                                        bool have0 = false;
                                        bool have1 = false;
                                        bool flip = false;
                                        std::uint32_t i0 = 0;
                                        std::uint32_t i1 = 0;
                                        int listFill = 0;
                                        std::uint32_t list0 = 0;
                                        std::uint32_t list1 = 0;
                                        for (std::uint32_t ii = 0; ii < numIndices; ++ii)
                                        {
                                            ++result.Drops.TokenTotal;
                                            std::size_t off = static_cast<std::size_t>(indexPtr.Offset) +
                                                static_cast<std::size_t>(ii) * bytesPerIndex;
                                            std::uint32_t idx = bytesPerIndex == 1 ? readIndexU8(off)
                                                : (bytesPerIndex == 2 ? readIndexU16(off) : readIndexU32(off));
                                            if (idx == 0u)
                                                ++result.Drops.ZeroIndex;
                                            bool restart = (bytesPerIndex == 1 && (idx == 0x00u || idx == 0xFFu)) ||
                                                           (bytesPerIndex == 2 && idx == 0xFFFFu) ||
                                                           (bytesPerIndex == 4 && idx == 0xFFFFFFFFu);
                                            if (restart || idx >= vertexCount)
                                            {
                                                if (restart)
                                                    ++result.Drops.Restart;
                                                else
                                                    ++result.Drops.OutOfRange;
                                                have0 = false;
                                                have1 = false;
                                                flip = false;
                                                listFill = 0;
                                                continue;
                                            }

                                            int mapped = remap[static_cast<std::size_t>(idx)];
                                            if (mapped < 0)
                                            {
                                                ++result.Drops.RemapDrop;
                                                have0 = false;
                                                have1 = false;
                                                flip = false;
                                                listFill = 0;
                                                continue;
                                            }
                                            std::uint32_t m = static_cast<std::uint32_t>(mapped);

                                            if (listFill == 0)
                                            {
                                                list0 = m;
                                                listFill = 1;
                                            }
                                            else if (listFill == 1)
                                            {
                                                list1 = m;
                                                listFill = 2;
                                            }
                                            else
                                            {
                                                if (list0 != list1 && list1 != m && list0 != m)
                                                {
                                                    listOut.push_back(list0);
                                                    listOut.push_back(list1);
                                                    listOut.push_back(m);
                                                }
                                                listFill = 0;
                                            }

                                            if (!have0)
                                            {
                                                i0 = m;
                                                have0 = true;
                                                continue;
                                            }
                                            if (!have1)
                                            {
                                                i1 = m;
                                                have1 = true;
                                                continue;
                                            }

                                            if (i0 != i1 && i1 != m && i0 != m)
                                            {
                                                if (flip)
                                                {
                                                    out.push_back(i1);
                                                    out.push_back(i0);
                                                    out.push_back(m);
                                                }
                                                else
                                                {
                                                    out.push_back(i0);
                                                    out.push_back(i1);
                                                    out.push_back(m);
                                                }
                                            }
                                            i0 = i1;
                                            i1 = m;
                                            flip = !flip;
                                        }
                                        result.ListIndices = std::move(listOut);
                                    }
                                    result.Indices = std::move(out);
                                    result.Score = bestScore;
                                    result.SampleCount = static_cast<int>(sampleCount);
                                    result.BytesPerIndex = bytesPerIndex;
                                    return result;
                                };

                                auto decodePackedHeaderStrip = [&]() -> IndexedDecodeResult {
                                    IndexedDecodeResult result;
                                    result.NumField = 0x1Cu;
                                    result.PtrField = 0x98u;

                                    std::uint32_t commandCount = 0;
                                    std::uint32_t commandRel = 0;
                                    if (!ReadBEU32(cpuData, packedStreamBase + 0x1C, commandCount) ||
                                        !ReadBEU32(cpuData, primitiveBase + 0x98, commandRel) ||
                                        commandCount < 3u || commandCount > 2000000u)
                                    {
                                        return result;
                                    }
                                    result.NumIndices = commandCount;
                                    result.IndexPtrRaw = commandRel;

                                    std::size_t commandScanBytes = 0;
                                    std::uint32_t commandEndRel = 0;
                                    if (ReadBEU32(cpuData, primitiveBase + 0x88, commandEndRel) &&
                                        commandEndRel > commandRel &&
                                        commandEndRel - commandRel <= 0x4000u &&
                                        inForest(commandRel, static_cast<std::size_t>(commandEndRel - commandRel)))
                                    {
                                        commandScanBytes = static_cast<std::size_t>(commandEndRel - commandRel);
                                    }

                                    std::size_t countBoundBytes = static_cast<std::size_t>(commandCount) * 2u;
                                    if (countBoundBytes >= 12u && inForest(commandRel, countBoundBytes))
                                    {
                                        if (commandScanBytes == 0u)
                                            commandScanBytes = countBoundBytes;
                                        else
                                            commandScanBytes = std::min(commandScanBytes, countBoundBytes);
                                    }

                                    if (commandScanBytes < 12u || !inForest(commandRel, commandScanBytes))
                                        return result;

                                    commandScanBytes = std::min<std::size_t>(commandScanBytes, 0x800u);

                                    std::vector<std::uint32_t> out;
                                    out.reserve(static_cast<std::size_t>(vertexCount) * 3u);
                                    std::size_t acceptedCommands = 0;

                                    for (std::size_t off = 0; off + 12u <= commandScanBytes; off += 4u)
                                    {
                                        std::uint32_t op = 0;
                                        std::uint32_t start = 0;
                                        std::uint32_t count = 0;
                                        if (!ReadBEU32(cpuData, forestBase + static_cast<std::size_t>(commandRel) + off, op) ||
                                            !ReadBEU32(cpuData, forestBase + static_cast<std::size_t>(commandRel) + off + 4u, start) ||
                                            !ReadBEU32(cpuData, forestBase + static_cast<std::size_t>(commandRel) + off + 8u, count))
                                        {
                                            break;
                                        }

                                        if ((op >> 24) != 0x22u)
                                            continue;
                                        if (count < 3u || start >= vertexCount || count > vertexCount || start + count > vertexCount)
                                            continue;

                                        bool flip = false;
                                        std::size_t before = out.size();
                                        for (std::uint32_t vi = start + 2u; vi < start + count; ++vi)
                                        {
                                            ++result.Drops.TokenTotal;
                                            std::uint32_t i0Raw = vi - 2u;
                                            std::uint32_t i1Raw = vi - 1u;
                                            std::uint32_t i2Raw = vi;
                                            if (i0Raw == 0u || i1Raw == 0u || i2Raw == 0u)
                                                ++result.Drops.ZeroIndex;
                                            int i0 = remap[static_cast<std::size_t>(i0Raw)];
                                            int i1 = remap[static_cast<std::size_t>(i1Raw)];
                                            int i2 = remap[static_cast<std::size_t>(i2Raw)];
                                            if (i0 < 0 || i1 < 0 || i2 < 0)
                                            {
                                                ++result.Drops.RemapDrop;
                                                flip = !flip;
                                                continue;
                                            }
                                            if (i0 == i1 || i1 == i2 || i0 == i2)
                                            {
                                                flip = !flip;
                                                continue;
                                            }

                                            if (flip)
                                            {
                                                out.push_back(static_cast<std::uint32_t>(i1));
                                                out.push_back(static_cast<std::uint32_t>(i0));
                                                out.push_back(static_cast<std::uint32_t>(i2));
                                            }
                                            else
                                            {
                                                out.push_back(static_cast<std::uint32_t>(i0));
                                                out.push_back(static_cast<std::uint32_t>(i1));
                                                out.push_back(static_cast<std::uint32_t>(i2));
                                            }
                                            flip = !flip;
                                        }

                                        if (out.size() > before)
                                            ++acceptedCommands;
                                    }

                                    if (out.size() < 3u || acceptedCommands == 0u)
                                        return result;

                                    result.Indices = std::move(out);
                                    result.Score = static_cast<int>(acceptedCommands * 8u);
                                    result.SampleCount = static_cast<int>(std::min<std::size_t>(acceptedCommands, 512u));
                                    result.BytesPerIndex = 2u;
                                    return result;
                                };

                                std::array<std::size_t, 3> primitiveFieldCandidates{
                                    0x8Cu, 0x90u, 0x94u
                                };

                                IndexedDecodeResult bestIndexed{};
                                if (enableExperimentalPackedPrimitiveIndex)
                                {
                                    for (std::size_t numField : primitiveFieldCandidates)
                                    {
                                        for (std::size_t ptrField : primitiveFieldCandidates)
                                        {
                                            if (numField == ptrField)
                                                continue;
                                            IndexedDecodeResult candidate = decodeIndexedStrip(numField, ptrField);
                                            if (candidate.Indices.size() < 3)
                                                continue;
                                            if (candidate.Score > bestIndexed.Score ||
                                                (candidate.Score == bestIndexed.Score &&
                                                 candidate.Indices.size() > bestIndexed.Indices.size()))
                                            {
                                                bestIndexed = std::move(candidate);
                                            }
                                        }
                                    }
                                }
                                {
                                    IndexedDecodeResult candidate = decodePackedHeaderStrip();
                                    if (candidate.Indices.size() >= 3 &&
                                        (candidate.Score > bestIndexed.Score ||
                                         (candidate.Score == bestIndexed.Score &&
                                          candidate.Indices.size() > bestIndexed.Indices.size())))
                                    {
                                        bestIndexed = std::move(candidate);
                                    }
                                }

                                std::vector<std::uint32_t> indexedIndices = std::move(bestIndexed.Indices);
                                std::vector<std::uint32_t> indexedListIndices = std::move(bestIndexed.ListIndices);
                                std::size_t indexedBytesPerIndex = bestIndexed.BytesPerIndex;
                                bool indexedUsedList = false;
                                if (indexedIndices.size() / 3u > static_cast<std::size_t>(vertexCount) * 4u)
                                    indexedIndices.clear();

                                std::uint32_t packedCommandBytes = 0;
                                ReadBEU32(cpuData, packedStreamBase + 0x1Cu, packedCommandBytes);
                                int fallbackMode = experimentalFallbackMode;
                                if (fallbackMode == 3)
                                    fallbackMode = (packedCommandBytes > 16u) ? 1 : 0;

                                std::vector<std::uint32_t> fallbackStripIndices;
                                fallbackStripIndices.reserve(static_cast<std::size_t>(vertexCount) * 3u);
                                for (auto const& range : fallbackRanges)
                                {
                                    std::uint32_t begin = range.first;
                                    std::uint32_t end = range.second;
                                    if (end < begin + 3u)
                                        continue;

                                    bool flip = false;
                                    for (std::uint32_t vi = begin + 2u; vi < end; ++vi)
                                    {
                                        int i0 = remap[static_cast<std::size_t>(vi - 2)];
                                        int i1 = remap[static_cast<std::size_t>(vi - 1)];
                                        int i2 = remap[static_cast<std::size_t>(vi)];
                                        if (i0 < 0 || i1 < 0 || i2 < 0)
                                        {
                                            if (enableExperimentalFallbackRestartOnGap)
                                                flip = false;
                                            else
                                                flip = !flip;
                                            continue;
                                        }
                                        if (i0 == i1 || i1 == i2 || i0 == i2)
                                        {
                                            if (enableExperimentalFallbackRestartOnGap)
                                                flip = false;
                                            else
                                                flip = !flip;
                                            continue;
                                        }

                                        if (flip)
                                        {
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i1));
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i0));
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i2));
                                        }
                                        else
                                        {
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i0));
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i1));
                                            fallbackStripIndices.push_back(static_cast<std::uint32_t>(i2));
                                        }
                                        flip = !flip;
                                    }
                                }

                                std::vector<std::uint32_t> fallbackListIndices;
                                fallbackListIndices.reserve(static_cast<std::size_t>(vertexCount));
                                auto emitListBlock = [&](std::uint32_t begin, std::uint32_t end) {
                                    for (std::uint32_t vi = begin; vi + 2u < end; vi += 3u)
                                    {
                                        int i0 = remap[static_cast<std::size_t>(vi + 0u)];
                                        int i1 = remap[static_cast<std::size_t>(vi + 1u)];
                                        int i2 = remap[static_cast<std::size_t>(vi + 2u)];
                                        if (i0 < 0 || i1 < 0 || i2 < 0)
                                            continue;
                                        if (i0 == i1 || i1 == i2 || i0 == i2)
                                            continue;
                                        fallbackListIndices.push_back(static_cast<std::uint32_t>(i0));
                                        fallbackListIndices.push_back(static_cast<std::uint32_t>(i1));
                                        fallbackListIndices.push_back(static_cast<std::uint32_t>(i2));
                                    }
                                };

                                for (auto const& range : fallbackRanges)
                                    emitListBlock(range.first, range.second);

                                std::vector<std::uint32_t> fallbackIndices =
                                    (fallbackMode == 1)
                                    ? fallbackListIndices
                                    : fallbackStripIndices;

                                auto dominantIndexRatio = [&](std::vector<std::uint32_t> const& candidate) -> float {
                                    if (candidate.size() < 12u)
                                        return 0.0f;
                                    std::size_t triCount = candidate.size() / 3u;
                                    std::size_t sampleTriCount = std::min<std::size_t>(triCount, 2048u);
                                    if (sampleTriCount == 0u)
                                        return 0.0f;

                                    std::vector<std::uint32_t> sampleRefs;
                                    sampleRefs.reserve(sampleTriCount * 3u);
                                    for (std::size_t si = 0; si < sampleTriCount; ++si)
                                    {
                                        std::size_t ti = (sampleTriCount == triCount)
                                            ? si
                                            : ((si * triCount) / sampleTriCount);
                                        sampleRefs.push_back(candidate[ti * 3u + 0u]);
                                        sampleRefs.push_back(candidate[ti * 3u + 1u]);
                                        sampleRefs.push_back(candidate[ti * 3u + 2u]);
                                    }
                                    if (sampleRefs.size() < 12u)
                                        return 0.0f;

                                    std::sort(sampleRefs.begin(), sampleRefs.end());
                                    std::size_t maxRun = 1u;
                                    std::size_t run = 1u;
                                    for (std::size_t i = 1; i < sampleRefs.size(); ++i)
                                    {
                                        if (sampleRefs[i] == sampleRefs[i - 1u])
                                        {
                                            ++run;
                                            if (run > maxRun)
                                                maxRun = run;
                                        }
                                        else
                                        {
                                            run = 1u;
                                        }
                                    }
                                    return static_cast<float>(maxRun) /
                                        static_cast<float>(sampleRefs.size());
                                };

                                auto indexCoverageRatio = [&](std::vector<std::uint32_t> const& candidate) -> float {
                                    if (candidate.size() < 3u || vertices.empty())
                                        return 0.0f;
                                    std::vector<std::uint8_t> used(vertices.size(), 0u);
                                    std::size_t unique = 0u;
                                    for (std::uint32_t idx : candidate)
                                    {
                                        std::size_t i = static_cast<std::size_t>(idx);
                                        if (i >= used.size() || used[i] != 0u)
                                            continue;
                                        used[i] = 1u;
                                        ++unique;
                                    }
                                    return static_cast<float>(unique) /
                                        static_cast<float>(vertices.size());
                                };

                                auto scoreIndexQuality = [&](std::vector<std::uint32_t> const& candidate) -> float {
                                    if (candidate.size() < 3u)
                                        return std::numeric_limits<float>::infinity();

                                    std::size_t triCount = candidate.size() / 3u;
                                    std::size_t sampleTriCount = std::min<std::size_t>(triCount, 512u);
                                    struct TriEdges
                                    {
                                        float L0 = 0.0f;
                                        float L1 = 0.0f;
                                        float L2 = 0.0f;
                                    };
                                    std::vector<TriEdges> triEdges;
                                    triEdges.reserve(sampleTriCount);
                                    std::vector<float> edgeLengths;
                                    edgeLengths.reserve(sampleTriCount * 3u);
                                    for (std::size_t si = 0; si < sampleTriCount; ++si)
                                    {
                                        std::size_t ti = (sampleTriCount == triCount)
                                            ? si
                                            : ((si * triCount) / sampleTriCount);
                                        std::size_t i0 = static_cast<std::size_t>(candidate[ti * 3u + 0u]);
                                        std::size_t i1 = static_cast<std::size_t>(candidate[ti * 3u + 1u]);
                                        std::size_t i2 = static_cast<std::size_t>(candidate[ti * 3u + 2u]);
                                        if (i0 >= vertices.size() || i1 >= vertices.size() || i2 >= vertices.size())
                                            continue;

                                        auto edgeLen = [&](std::size_t a, std::size_t b) -> float {
                                            Vector3 const& pa = vertices[a].Pos;
                                            Vector3 const& pb = vertices[b].Pos;
                                            float dx = pa.X - pb.X;
                                            float dy = pa.Y - pb.Y;
                                            float dz = pa.Z - pb.Z;
                                            float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                                            return std::isfinite(len) ? len : std::numeric_limits<float>::infinity();
                                        };

                                        float l0 = edgeLen(i0, i1);
                                        float l1 = edgeLen(i1, i2);
                                        float l2 = edgeLen(i2, i0);
                                        if (!std::isfinite(l0) || !std::isfinite(l1) || !std::isfinite(l2))
                                            continue;

                                        edgeLengths.push_back(l0);
                                        edgeLengths.push_back(l1);
                                        edgeLengths.push_back(l2);
                                        triEdges.push_back({l0, l1, l2});
                                    }

                                    if (edgeLengths.empty() || triEdges.empty())
                                        return std::numeric_limits<float>::infinity();

                                    std::nth_element(edgeLengths.begin(),
                                                     edgeLengths.begin() + static_cast<std::ptrdiff_t>(edgeLengths.size() / 2u),
                                                     edgeLengths.end());
                                    float median = edgeLengths[edgeLengths.size() / 2u];
                                    float threshold = std::max(100.0f, median * 12.0f);
                                    std::size_t longCount = 0;
                                    std::size_t badAspectCount = 0;
                                    constexpr float kMinEdge = 1.0e-5f;
                                    constexpr float kAspectThreshold = 25.0f;
                                    for (auto const& tri : triEdges)
                                    {
                                        if (tri.L0 > threshold)
                                            ++longCount;
                                        if (tri.L1 > threshold)
                                            ++longCount;
                                        if (tri.L2 > threshold)
                                            ++longCount;

                                        float minEdge = std::min(tri.L0, std::min(tri.L1, tri.L2));
                                        float maxEdge = std::max(tri.L0, std::max(tri.L1, tri.L2));
                                        if (minEdge < kMinEdge || (maxEdge / minEdge) > kAspectThreshold)
                                            ++badAspectCount;
                                    }

                                    float longRatio = static_cast<float>(longCount) /
                                                      static_cast<float>(triEdges.size() * 3u);
                                    float aspectRatio = static_cast<float>(badAspectCount) /
                                                        static_cast<float>(triEdges.size());
                                    float dominantPenalty = 0.0f;
                                    float dominantRatio = dominantIndexRatio(candidate);
                                    if (dominantRatio > 0.12f)
                                    {
                                        dominantPenalty = (dominantRatio - 0.12f) * 2.5f;
                                    }
                                    float aspectPenalty = aspectRatio * 2.0f;
                                    // Prefer denser candidates when quality is otherwise similar.
                                    float sparsityPenalty = 0.0f;
                                    if (triCount > 0u)
                                    {
                                        sparsityPenalty =
                                            std::clamp(64.0f / static_cast<float>(triCount), 0.0f, 0.25f) * 0.02f;
                                    }
                                    float coveragePenalty = 0.0f;
                                    float coverage = indexCoverageRatio(candidate);
                                    if (coverage < 0.12f)
                                        coveragePenalty = (0.12f - coverage) * experimentalCoverageWeight;
                                    return longRatio + aspectPenalty + sparsityPenalty + dominantPenalty + coveragePenalty;
                                };

                                if (!indexedIndices.empty() && dominantIndexRatio(indexedIndices) > 0.18f)
                                    indexedIndices.clear();
                                if (!indexedListIndices.empty() && dominantIndexRatio(indexedListIndices) > 0.18f)
                                    indexedListIndices.clear();

                                if (!indexedIndices.empty() && !indexedListIndices.empty())
                                {
                                    float stripScore = scoreIndexQuality(indexedIndices);
                                    float listScore = scoreIndexQuality(indexedListIndices);
                                    std::size_t stripTriangles = indexedIndices.size() / 3u;
                                    std::size_t listTriangles = indexedListIndices.size() / 3u;
                                    bool chooseList = false;
                                    // Prefer list decode only if it is clearly cleaner and keeps enough density.
                                    if (listTriangles * 10u >= stripTriangles * 8u &&
                                        listScore + 0.002f < stripScore)
                                    {
                                        chooseList = true;
                                    }
                                    else if (listTriangles * 10u >= stripTriangles * 6u &&
                                             listScore + 0.010f < stripScore)
                                    {
                                        chooseList = true;
                                    }

                                    if (chooseList)
                                    {
                                        indexedIndices = std::move(indexedListIndices);
                                        indexedUsedList = true;
                                    }
                                }
                                else if (indexedIndices.empty() && !indexedListIndices.empty())
                                {
                                    indexedIndices = std::move(indexedListIndices);
                                    indexedUsedList = true;
                                }

                                auto decodePackedMaskedStrip = [&](std::size_t countField,
                                                                   std::size_t ptrField,
                                                                   std::uint16_t mask) -> std::vector<std::uint32_t> {
                                    std::vector<std::uint32_t> out;

                                    std::uint32_t countValue = 0;
                                    std::uint32_t indexRel = 0;
                                    if (!ReadBEU32(cpuData, packedStreamBase + countField, countValue) ||
                                        !ReadBEU32(cpuData, packedStreamBase + ptrField, indexRel))
                                    {
                                        return out;
                                    }

                                    struct PackedCountMode
                                    {
                                        std::uint32_t EntryCount = 0;
                                        int ValidScore = -1;
                                    };

                                    auto scorePackedMode = [&](std::uint32_t entryCount) -> int {
                                        if (entryCount < 3u || entryCount > 2000000u)
                                            return -1;
                                        std::size_t sampleCount = std::min<std::size_t>(entryCount, 512u);
                                        std::size_t indexBase = forestBase + static_cast<std::size_t>(indexRel);
                                        int valid = 0;
                                        for (std::size_t ii = 0; ii < sampleCount; ++ii)
                                        {
                                            std::uint16_t raw = 0;
                                            if (!ReadBEU16(cpuData, indexBase + ii * 2u, raw))
                                                break;
                                            if (raw == 0xFFFFu)
                                            {
                                                ++valid;
                                                continue;
                                            }
                                            std::uint32_t idx = static_cast<std::uint32_t>(raw & mask);
                                            if (idx < vertexCount)
                                                ++valid;
                                        }
                                        return valid;
                                    };

                                    PackedCountMode bestMode{};
                                    auto considerMode = [&](std::uint32_t entryCount, std::size_t indexBytes) {
                                        if (entryCount < 3u || entryCount > 2000000u)
                                            return;
                                        if (!inForest(indexRel, indexBytes))
                                            return;
                                        int validScore = scorePackedMode(entryCount);
                                        if (validScore < 0)
                                            return;
                                        if (bestMode.ValidScore < validScore ||
                                            (bestMode.ValidScore == validScore && entryCount > bestMode.EntryCount))
                                        {
                                            bestMode.EntryCount = entryCount;
                                            bestMode.ValidScore = validScore;
                                        }
                                    };

                                    // Packed streams differ: this field can be either u16 count or raw byte size.
                                    if ((countValue & 1u) == 0u)
                                        considerMode(countValue / 2u, static_cast<std::size_t>(countValue));
                                    {
                                        std::uint64_t countBytes64 = static_cast<std::uint64_t>(countValue) * 2u;
                                        if (countBytes64 <= static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()))
                                        {
                                            considerMode(countValue, static_cast<std::size_t>(countBytes64));
                                        }
                                    }

                                    if (bestMode.EntryCount < 3u || bestMode.ValidScore < 0)
                                        return out;
                                    std::size_t sampleCount = std::min<std::size_t>(bestMode.EntryCount, 512u);
                                    if (sampleCount == 0u || bestMode.ValidScore * 5 < static_cast<int>(sampleCount))
                                        return out;

                                    std::size_t indexBase = forestBase + static_cast<std::size_t>(indexRel);
                                    out.reserve(static_cast<std::size_t>(bestMode.EntryCount) * 3u);

                                    bool have0 = false;
                                    bool have1 = false;
                                    bool flip = false;
                                    std::uint32_t i0 = 0;
                                    std::uint32_t i1 = 0;
                                    for (std::uint32_t ii = 0; ii < bestMode.EntryCount; ++ii)
                                    {
                                        std::uint16_t raw = 0;
                                        if (!ReadBEU16(cpuData, indexBase + static_cast<std::size_t>(ii) * 2u, raw))
                                            break;

                                        std::uint32_t idx = (raw == 0xFFFFu) ? 0xFFFFu : static_cast<std::uint32_t>(raw & mask);
                                        if (idx == 0xFFFFu || idx >= vertexCount)
                                        {
                                            have0 = false;
                                            have1 = false;
                                            flip = false;
                                            continue;
                                        }

                                        int mapped = remap[static_cast<std::size_t>(idx)];
                                        if (mapped < 0)
                                        {
                                            have0 = false;
                                            have1 = false;
                                            flip = false;
                                            continue;
                                        }

                                        std::uint32_t m = static_cast<std::uint32_t>(mapped);
                                        if (!have0)
                                        {
                                            i0 = m;
                                            have0 = true;
                                            continue;
                                        }
                                        if (!have1)
                                        {
                                            i1 = m;
                                            have1 = true;
                                            continue;
                                        }

                                        if (i0 != i1 && i1 != m && i0 != m)
                                        {
                                            if (flip)
                                            {
                                                out.push_back(i1);
                                                out.push_back(i0);
                                                out.push_back(m);
                                            }
                                            else
                                            {
                                                out.push_back(i0);
                                                out.push_back(i1);
                                                out.push_back(m);
                                            }
                                        }
                                        i0 = i1;
                                        i1 = m;
                                        flip = !flip;
                                    }

                                    return out;
                                };

                                auto decodePacked10BitStrip = [&](std::size_t bytesField,
                                                                  std::size_t ptrField,
                                                                  bool lsbFirst,
                                                                  Packed10IndexStyle style,
                                                                  Packed10PrimitiveMode primitiveMode) -> StripDecodeResult {
                                    StripDecodeResult result;
                                    std::vector<std::uint32_t>& out = result.Indices;
                                    std::uint32_t packedBytes = 0;
                                    std::uint32_t packedRel = 0;
                                    if (!ReadBEU32(cpuData, packedStreamBase + bytesField, packedBytes) ||
                                        !ReadBEU32(cpuData, packedStreamBase + ptrField, packedRel))
                                    {
                                        return result;
                                    }

                                    if (packedBytes < 8u || packedBytes > 0x200000u ||
                                        !inForest(packedRel, static_cast<std::size_t>(packedBytes)))
                                    {
                                        return result;
                                    }

                                    std::size_t packedBase = forestBase + static_cast<std::size_t>(packedRel);
                                    out.reserve(static_cast<std::size_t>(packedBytes / 2u) * 3u);

                                    bool stripHave0 = false;
                                    bool stripHave1 = false;
                                    bool stripFlip = false;
                                    std::uint32_t stripI0 = 0;
                                    std::uint32_t stripI1 = 0;
                                    int listFill = 0;
                                    std::uint32_t listI0 = 0;
                                    std::uint32_t listI1 = 0;
                                    std::uint64_t bitBuffer = 0;
                                    int bitCount = 0;
                                    std::size_t emittedSymbols = 0;
                                    constexpr std::size_t kMaxSymbols = 2'000'000u;

                                    auto restartPrimitive = [&]() {
                                        stripHave0 = false;
                                        stripHave1 = false;
                                        stripFlip = false;
                                        listFill = 0;
                                    };

                                    auto pushMappedIndex = [&](std::uint32_t mapped) {
                                        if (primitiveMode == Packed10PrimitiveMode::List)
                                        {
                                            if (listFill == 0)
                                            {
                                                listI0 = mapped;
                                                listFill = 1;
                                                return;
                                            }
                                            if (listFill == 1)
                                            {
                                                listI1 = mapped;
                                                listFill = 2;
                                                return;
                                            }

                                            if (listI0 != listI1 && listI1 != mapped && listI0 != mapped)
                                            {
                                                out.push_back(listI0);
                                                out.push_back(listI1);
                                                out.push_back(mapped);
                                            }
                                            listFill = 0;
                                            return;
                                        }

                                        if (!stripHave0)
                                        {
                                            stripI0 = mapped;
                                            stripHave0 = true;
                                            return;
                                        }
                                        if (!stripHave1)
                                        {
                                            stripI1 = mapped;
                                            stripHave1 = true;
                                            return;
                                        }

                                        if (stripI0 != stripI1 && stripI1 != mapped && stripI0 != mapped)
                                        {
                                            if (stripFlip)
                                            {
                                                out.push_back(stripI1);
                                                out.push_back(stripI0);
                                                out.push_back(mapped);
                                            }
                                            else
                                            {
                                                out.push_back(stripI0);
                                                out.push_back(stripI1);
                                                out.push_back(mapped);
                                            }
                                        }
                                        else
                                        {
                                            restartPrimitive();
                                            return;
                                        }
                                        stripI0 = stripI1;
                                        stripI1 = mapped;
                                        stripFlip = !stripFlip;
                                    };

                                    auto pushIndex = [&](std::uint32_t idx) {
                                        ++result.Drops.TokenTotal;
                                        if (idx == 0u)
                                            ++result.Drops.ZeroIndex;

                                        bool zeroIsRestart = (style == Packed10IndexStyle::DirectZeroRestart ||
                                                              style == Packed10IndexStyle::OneBasedZeroRestart);
                                        bool restart = (idx == 0x03FFu) || (zeroIsRestart && idx == 0u);
                                        std::uint32_t decodedIdx = idx;
                                        if (style == Packed10IndexStyle::OneBasedZeroRestart && idx != 0x03FFu)
                                        {
                                            if (idx == 0u)
                                                restart = true;
                                            else
                                                decodedIdx = idx - 1u;
                                        }

                                        if (restart || decodedIdx >= vertexCount)
                                        {
                                            if (restart)
                                                ++result.Drops.Restart;
                                            else
                                                ++result.Drops.OutOfRange;
                                            restartPrimitive();
                                            return;
                                        }

                                        int mapped = remap[static_cast<std::size_t>(decodedIdx)];
                                        if (mapped < 0)
                                        {
                                            ++result.Drops.RemapDrop;
                                            restartPrimitive();
                                            return;
                                        }

                                        pushMappedIndex(static_cast<std::uint32_t>(mapped));
                                    };

                                    for (std::uint32_t bi = 0; bi < packedBytes && emittedSymbols < kMaxSymbols; ++bi)
                                    {
                                        std::uint8_t byte = cpuData[packedBase + static_cast<std::size_t>(bi)];
                                        if (lsbFirst)
                                        {
                                            bitBuffer |= static_cast<std::uint64_t>(byte) << bitCount;
                                            bitCount += 8;
                                            while (bitCount >= 10 && emittedSymbols < kMaxSymbols)
                                            {
                                                std::uint32_t symbol = static_cast<std::uint32_t>(bitBuffer & 0x03FFu);
                                                bitBuffer >>= 10;
                                                bitCount -= 10;
                                                ++emittedSymbols;
                                                pushIndex(symbol);
                                            }
                                        }
                                        else
                                        {
                                            bitBuffer = (bitBuffer << 8) | byte;
                                            bitCount += 8;
                                            while (bitCount >= 10 && emittedSymbols < kMaxSymbols)
                                            {
                                                int shift = bitCount - 10;
                                                std::uint32_t symbol =
                                                    static_cast<std::uint32_t>((bitBuffer >> shift) & 0x03FFu);
                                                ++emittedSymbols;
                                                pushIndex(symbol);
                                                if (shift == 0)
                                                {
                                                    bitBuffer = 0;
                                                    bitCount = 0;
                                                }
                                                else
                                                {
                                                    bitBuffer &= ((static_cast<std::uint64_t>(1) << shift) - 1u);
                                                    bitCount = shift;
                                                }
                                            }
                                        }
                                    }

                                    return result;
                                };

                                std::vector<std::uint32_t> encodedIndices;
                                float encodedScore = std::numeric_limits<float>::infinity();
                                std::uint16_t encodedMask = 0u;
                                auto considerEncoded = [&](std::size_t countField,
                                                           std::size_t ptrField,
                                                           std::uint16_t mask) {
                                    std::vector<std::uint32_t> candidate =
                                        decodePackedMaskedStrip(countField, ptrField, mask);
                                    if (candidate.size() < 3u)
                                        return;
                                    float score = scoreIndexQuality(candidate);
                                    if (!std::isfinite(score))
                                        return;
                                    if (encodedIndices.empty() || score < encodedScore ||
                                        (std::abs(score - encodedScore) <= 0.002f &&
                                         candidate.size() > encodedIndices.size()))
                                    {
                                        encodedScore = score;
                                        encodedIndices = std::move(candidate);
                                        encodedMask = mask;
                                    }
                                };
                                if (enableExperimentalEncodedMask)
                                {
                                    // Some PS3 packed streams encode indices in bitfields inside u16 words.
                                    // Decode from the observed packed index payload (bytes/count can vary by asset).
                                    considerEncoded(0x1Cu, 0x3Cu, 0x03FFu);
                                    considerEncoded(0x1Cu, 0x3Cu, 0x00FFu);
                                }

                                std::vector<std::uint32_t> packed10BitIndices;
                                float packed10BitScore = std::numeric_limits<float>::infinity();
                                bool packed10BitLsb = true;
                                bool packed10BitOneBased = false;
                                bool packed10BitZeroRestart = false;
                                bool packed10BitList = false;
                                float packed10BitDropRatio = 0.0f;
                                Ps3DecodeDropStats packed10BitDrops{};
                                struct Packed10Candidate
                                {
                                    std::vector<std::uint32_t> Indices;
                                    float Score = std::numeric_limits<float>::infinity();
                                    float DropRatio = 1.0f;
                                    bool LsbFirst = true;
                                    bool OneBased = false;
                                    bool ZeroRestart = false;
                                    bool List = false;
                                    Ps3DecodeDropStats Drops{};
                                };
                                Packed10Candidate bestPacked10Strip{};
                                Packed10Candidate bestPacked10List{};
                                auto packedDropRatio = [](Ps3DecodeDropStats const& s) -> float {
                                    if (s.TokenTotal == 0u)
                                        return 0.0f;
                                    std::size_t dropped = s.Restart + s.OutOfRange + s.RemapDrop;
                                    return static_cast<float>(dropped) /
                                        static_cast<float>(s.TokenTotal);
                                };
                                auto considerPacked10Bit = [&](bool lsbFirst,
                                                               Packed10IndexStyle style,
                                                               Packed10PrimitiveMode primitiveMode) {
                                    StripDecodeResult decoded =
                                        decodePacked10BitStrip(0x1Cu, 0x14u, lsbFirst, style, primitiveMode);
                                    std::vector<std::uint32_t> candidate = std::move(decoded.Indices);
                                    if (candidate.size() < 3u)
                                        return;
                                    float dom = dominantIndexRatio(candidate);
                                    float cov = indexCoverageRatio(candidate);
                                    if (dom > 0.18f || cov < 0.10f)
                                        return;
                                    float drop = packedDropRatio(decoded.Drops);
                                    if (drop > 0.70f)
                                        return;
                                    float score = scoreIndexQuality(candidate);
                                    if (!std::isfinite(score))
                                        return;
                                    Packed10Candidate cand;
                                    cand.Indices = std::move(candidate);
                                    cand.Score = score;
                                    cand.DropRatio = drop;
                                    cand.LsbFirst = lsbFirst;
                                    cand.OneBased = (style == Packed10IndexStyle::OneBasedZeroRestart);
                                    cand.ZeroRestart = (style == Packed10IndexStyle::DirectZeroRestart ||
                                                        style == Packed10IndexStyle::OneBasedZeroRestart);
                                    cand.List = (primitiveMode == Packed10PrimitiveMode::List);
                                    cand.Drops = decoded.Drops;

                                    Packed10Candidate& best =
                                        (primitiveMode == Packed10PrimitiveMode::List)
                                        ? bestPacked10List
                                        : bestPacked10Strip;

                                    float adjustedScore = cand.Score + cand.DropRatio * 0.75f;
                                    float bestAdjusted = best.Score + best.DropRatio * 0.75f;
                                    if (best.Indices.empty() || adjustedScore < bestAdjusted ||
                                        (std::abs(adjustedScore - bestAdjusted) <= 0.002f &&
                                         cand.Indices.size() > best.Indices.size()))
                                    {
                                        best = std::move(cand);
                                    }
                                };
                                for (Packed10PrimitiveMode primitiveMode : {Packed10PrimitiveMode::Strip,
                                                                            Packed10PrimitiveMode::List})
                                {
                                    considerPacked10Bit(true, Packed10IndexStyle::Direct, primitiveMode);
                                    considerPacked10Bit(false, Packed10IndexStyle::Direct, primitiveMode);
                                    considerPacked10Bit(true, Packed10IndexStyle::DirectZeroRestart, primitiveMode);
                                    considerPacked10Bit(false, Packed10IndexStyle::DirectZeroRestart, primitiveMode);
                                    considerPacked10Bit(true, Packed10IndexStyle::OneBasedZeroRestart, primitiveMode);
                                    considerPacked10Bit(false, Packed10IndexStyle::OneBasedZeroRestart, primitiveMode);
                                }

                                auto adoptPacked10 = [&](Packed10Candidate&& cand) {
                                    packed10BitIndices = std::move(cand.Indices);
                                    packed10BitScore = cand.Score;
                                    packed10BitLsb = cand.LsbFirst;
                                    packed10BitOneBased = cand.OneBased;
                                    packed10BitZeroRestart = cand.ZeroRestart;
                                    packed10BitList = cand.List;
                                    packed10BitDropRatio = cand.DropRatio;
                                    packed10BitDrops = cand.Drops;
                                };

                                if (!bestPacked10Strip.Indices.empty() && !bestPacked10List.Indices.empty())
                                {
                                    float stripAdjusted = bestPacked10Strip.Score + bestPacked10Strip.DropRatio * 0.75f;
                                    float listAdjusted = bestPacked10List.Score + bestPacked10List.DropRatio * 0.75f;
                                    std::size_t stripTriangles = bestPacked10Strip.Indices.size() / 3u;
                                    std::size_t listTriangles = bestPacked10List.Indices.size() / 3u;
                                    bool chooseList = false;
                                    // Only prefer list decode when it wins clearly and keeps enough triangle density.
                                    if (listTriangles * 10u >= stripTriangles * 8u &&
                                        listAdjusted + 0.002f < stripAdjusted)
                                    {
                                        chooseList = true;
                                    }
                                    else if (listTriangles * 10u >= stripTriangles * 6u &&
                                             listAdjusted + 0.010f < stripAdjusted)
                                    {
                                        chooseList = true;
                                    }

                                    if (chooseList)
                                        adoptPacked10(std::move(bestPacked10List));
                                    else
                                        adoptPacked10(std::move(bestPacked10Strip));
                                }
                                else if (!bestPacked10Strip.Indices.empty())
                                {
                                    adoptPacked10(std::move(bestPacked10Strip));
                                }
                                else if (!bestPacked10List.Indices.empty())
                                {
                                    adoptPacked10(std::move(bestPacked10List));
                                }

                                std::vector<std::uint32_t> indirectIndices;
                                float indirectScore = std::numeric_limits<float>::infinity();
                                std::string indirectMode;
                                auto decodeIndirectCommandPayload = [&]() {
                                    std::uint32_t commandRel = 0;
                                    if (!ReadBEU32(cpuData, primitiveBase + 0x98u, commandRel) ||
                                        !inForest(commandRel, 0x14u))
                                    {
                                        return;
                                    }

                                    std::size_t commandBase = forestBase + static_cast<std::size_t>(commandRel);
                                    std::uint32_t payloadPtrRaw = 0;
                                    std::uint32_t payloadType = 0;
                                    std::uint32_t payloadBytes = 0;
                                    std::uint32_t payloadEntryCount = 0;
                                    if (!ReadBEU32(cpuData, commandBase + 0x0u, payloadPtrRaw) ||
                                        !ReadBEU32(cpuData, commandBase + 0x4u, payloadType) ||
                                        !ReadBEU32(cpuData, commandBase + 0x8u, payloadBytes) ||
                                        !ReadBEU32(cpuData, commandBase + 0x10u, payloadEntryCount))
                                    {
                                        return;
                                    }

                                    if (payloadBytes < 8u || payloadBytes > 0x200000u)
                                        return;
                                    if (payloadEntryCount == 0u || payloadEntryCount > 2'000'000u)
                                        return;

                                    Ps3RawPointer payloadPtr = decodePs3RawPointer(payloadPtrRaw);
                                    if (!payloadPtr.Valid)
                                        return;

                                    std::span<const std::uint8_t> payloadData = payloadPtr.IsGpu ? gpuData : cpuData;
                                    if (payloadPtr.Offset >= payloadData.size())
                                        return;

                                    if (static_cast<std::uint64_t>(payloadPtr.Offset) + payloadBytes > payloadData.size())
                                        return;

                                    std::size_t payloadBase = static_cast<std::size_t>(payloadPtr.Offset);
                                    std::size_t payloadSize = static_cast<std::size_t>(payloadBytes);

                                    auto decodeStripFromReader = [&](std::size_t entryCount,
                                                                     auto&& readIndex,
                                                                     std::uint32_t restartValue) -> std::vector<std::uint32_t> {
                                        std::vector<std::uint32_t> out;
                                        out.reserve(entryCount * 3u);

                                        bool have0 = false;
                                        bool have1 = false;
                                        bool flip = false;
                                        std::uint32_t i0 = 0;
                                        std::uint32_t i1 = 0;
                                        for (std::size_t ii = 0; ii < entryCount; ++ii)
                                        {
                                            std::uint32_t idx = readIndex(ii);
                                            if (idx == restartValue || idx >= vertexCount)
                                            {
                                                have0 = false;
                                                have1 = false;
                                                flip = false;
                                                continue;
                                            }

                                            int mapped = remap[static_cast<std::size_t>(idx)];
                                            if (mapped < 0)
                                            {
                                                have0 = false;
                                                have1 = false;
                                                flip = false;
                                                continue;
                                            }

                                            std::uint32_t m = static_cast<std::uint32_t>(mapped);
                                            if (!have0)
                                            {
                                                i0 = m;
                                                have0 = true;
                                                continue;
                                            }
                                            if (!have1)
                                            {
                                                i1 = m;
                                                have1 = true;
                                                continue;
                                            }

                                            if (i0 != i1 && i1 != m && i0 != m)
                                            {
                                                if (flip)
                                                {
                                                    out.push_back(i1);
                                                    out.push_back(i0);
                                                    out.push_back(m);
                                                }
                                                else
                                                {
                                                    out.push_back(i0);
                                                    out.push_back(i1);
                                                    out.push_back(m);
                                                }
                                            }
                                            i0 = i1;
                                            i1 = m;
                                            flip = !flip;
                                        }
                                        return out;
                                    };

                                    auto decodeListU16 = [&](std::size_t entryCount) -> std::vector<std::uint32_t> {
                                        std::vector<std::uint32_t> out;
                                        out.reserve(entryCount);
                                        for (std::size_t ii = 0; ii + 2u < entryCount; ii += 3u)
                                        {
                                            std::size_t o0 = payloadBase + ii * 2u;
                                            std::size_t o1 = payloadBase + (ii + 1u) * 2u;
                                            std::size_t o2 = payloadBase + (ii + 2u) * 2u;
                                            if (o2 + 2u > payloadData.size())
                                                break;

                                            std::uint32_t i0 = static_cast<std::uint32_t>(
                                                (payloadData[o0] << 8) | payloadData[o0 + 1u]);
                                            std::uint32_t i1 = static_cast<std::uint32_t>(
                                                (payloadData[o1] << 8) | payloadData[o1 + 1u]);
                                            std::uint32_t i2 = static_cast<std::uint32_t>(
                                                (payloadData[o2] << 8) | payloadData[o2 + 1u]);
                                            if (i0 == 0xFFFFu || i1 == 0xFFFFu || i2 == 0xFFFFu)
                                                continue;
                                            if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount)
                                                continue;

                                            int m0 = remap[static_cast<std::size_t>(i0)];
                                            int m1 = remap[static_cast<std::size_t>(i1)];
                                            int m2 = remap[static_cast<std::size_t>(i2)];
                                            if (m0 < 0 || m1 < 0 || m2 < 0)
                                                continue;
                                            if (m0 == m1 || m1 == m2 || m0 == m2)
                                                continue;

                                            out.push_back(static_cast<std::uint32_t>(m0));
                                            out.push_back(static_cast<std::uint32_t>(m1));
                                            out.push_back(static_cast<std::uint32_t>(m2));
                                        }
                                        return out;
                                    };

                                    auto decodePacked10FromPayload = [&](bool lsbFirst,
                                                                         Packed10IndexStyle style,
                                                                         Packed10PrimitiveMode primitiveMode,
                                                                         std::size_t maxSymbols) -> std::vector<std::uint32_t> {
                                        std::vector<std::uint32_t> out;
                                        out.reserve((payloadSize / 2u) * 3u);

                                        bool stripHave0 = false;
                                        bool stripHave1 = false;
                                        bool stripFlip = false;
                                        std::uint32_t stripI0 = 0;
                                        std::uint32_t stripI1 = 0;
                                        int listFill = 0;
                                        std::uint32_t listI0 = 0;
                                        std::uint32_t listI1 = 0;
                                        std::uint64_t bitBuffer = 0;
                                        int bitCount = 0;
                                        std::size_t emittedSymbols = 0;

                                        auto restartPrimitive = [&]() {
                                            stripHave0 = false;
                                            stripHave1 = false;
                                            stripFlip = false;
                                            listFill = 0;
                                        };

                                        auto pushMappedIndex = [&](std::uint32_t mapped) {
                                            if (primitiveMode == Packed10PrimitiveMode::List)
                                            {
                                                if (listFill == 0)
                                                {
                                                    listI0 = mapped;
                                                    listFill = 1;
                                                    return;
                                                }
                                                if (listFill == 1)
                                                {
                                                    listI1 = mapped;
                                                    listFill = 2;
                                                    return;
                                                }

                                                if (listI0 != listI1 && listI1 != mapped && listI0 != mapped)
                                                {
                                                    out.push_back(listI0);
                                                    out.push_back(listI1);
                                                    out.push_back(mapped);
                                                }
                                                listFill = 0;
                                                return;
                                            }

                                            if (!stripHave0)
                                            {
                                                stripI0 = mapped;
                                                stripHave0 = true;
                                                return;
                                            }
                                            if (!stripHave1)
                                            {
                                                stripI1 = mapped;
                                                stripHave1 = true;
                                                return;
                                            }

                                            if (stripI0 != stripI1 && stripI1 != mapped && stripI0 != mapped)
                                            {
                                                if (stripFlip)
                                                {
                                                    out.push_back(stripI1);
                                                    out.push_back(stripI0);
                                                    out.push_back(mapped);
                                                }
                                                else
                                                {
                                                    out.push_back(stripI0);
                                                    out.push_back(stripI1);
                                                    out.push_back(mapped);
                                                }
                                            }
                                            else
                                            {
                                                restartPrimitive();
                                                return;
                                            }
                                            stripI0 = stripI1;
                                            stripI1 = mapped;
                                            stripFlip = !stripFlip;
                                        };

                                        auto pushIndex = [&](std::uint32_t idx) {
                                            bool zeroIsRestart = (style == Packed10IndexStyle::DirectZeroRestart ||
                                                                  style == Packed10IndexStyle::OneBasedZeroRestart);
                                            bool restart = (idx == 0x03FFu) || (zeroIsRestart && idx == 0u);
                                            std::uint32_t decodedIdx = idx;
                                            if (style == Packed10IndexStyle::OneBasedZeroRestart && idx != 0x03FFu)
                                            {
                                                if (idx == 0u)
                                                {
                                                    restart = true;
                                                }
                                                else
                                                {
                                                    decodedIdx = idx - 1u;
                                                }
                                            }

                                            if (restart || decodedIdx >= vertexCount)
                                            {
                                                restartPrimitive();
                                                return;
                                            }

                                            int mapped = remap[static_cast<std::size_t>(decodedIdx)];
                                            if (mapped < 0)
                                            {
                                                restartPrimitive();
                                                return;
                                            }
                                            pushMappedIndex(static_cast<std::uint32_t>(mapped));
                                        };

                                        for (std::size_t bi = 0; bi < payloadSize && emittedSymbols < maxSymbols; ++bi)
                                        {
                                            std::uint8_t byte = payloadData[payloadBase + bi];
                                            if (lsbFirst)
                                            {
                                                bitBuffer |= static_cast<std::uint64_t>(byte) << bitCount;
                                                bitCount += 8;
                                                while (bitCount >= 10 && emittedSymbols < maxSymbols)
                                                {
                                                    std::uint32_t symbol = static_cast<std::uint32_t>(bitBuffer & 0x03FFu);
                                                    bitBuffer >>= 10;
                                                    bitCount -= 10;
                                                    ++emittedSymbols;
                                                    pushIndex(symbol);
                                                }
                                            }
                                            else
                                            {
                                                bitBuffer = (bitBuffer << 8) | byte;
                                                bitCount += 8;
                                                while (bitCount >= 10 && emittedSymbols < maxSymbols)
                                                {
                                                    int shift = bitCount - 10;
                                                    std::uint32_t symbol =
                                                        static_cast<std::uint32_t>((bitBuffer >> shift) & 0x03FFu);
                                                    ++emittedSymbols;
                                                    pushIndex(symbol);
                                                    if (shift == 0)
                                                    {
                                                        bitBuffer = 0;
                                                        bitCount = 0;
                                                    }
                                                    else
                                                    {
                                                        bitBuffer &= ((static_cast<std::uint64_t>(1) << shift) - 1u);
                                                        bitCount = shift;
                                                    }
                                                }
                                            }
                                        }

                                        return out;
                                    };

                                    auto considerIndirectCandidate = [&](std::vector<std::uint32_t> candidate,
                                                                          std::string_view mode) {
                                        if (candidate.size() < 3u)
                                            return;
                                        float dom = dominantIndexRatio(candidate);
                                        float cov = indexCoverageRatio(candidate);
                                        if (dom > 0.22f || cov < 0.03f)
                                            return;
                                        float score = scoreIndexQuality(candidate);
                                        if (!std::isfinite(score))
                                            return;
                                        if (indirectIndices.empty() || score < indirectScore ||
                                            (std::abs(score - indirectScore) <= 0.002f &&
                                             candidate.size() > indirectIndices.size()))
                                        {
                                            indirectScore = score;
                                            indirectIndices = std::move(candidate);
                                            indirectMode = std::string(mode);
                                        }
                                    };

                                    std::size_t payloadEntries = static_cast<std::size_t>(payloadEntryCount);
                                    if ((payloadSize % 2u) == 0u &&
                                        payloadEntries <= (payloadSize / 2u))
                                    {
                                        considerIndirectCandidate(
                                            decodeStripFromReader(
                                                payloadEntries,
                                                [&](std::size_t ii) -> std::uint32_t {
                                                    std::size_t o = payloadBase + ii * 2u;
                                                    return static_cast<std::uint32_t>(
                                                        (payloadData[o] << 8) | payloadData[o + 1u]);
                                                },
                                                0xFFFFu),
                                            "indirect_u16_strip");
                                        considerIndirectCandidate(decodeListU16(payloadEntries), "indirect_u16_list");
                                    }

                                    if ((payloadSize % 4u) == 0u &&
                                        payloadEntries <= (payloadSize / 4u))
                                    {
                                        considerIndirectCandidate(
                                            decodeStripFromReader(
                                                payloadEntries,
                                                [&](std::size_t ii) -> std::uint32_t {
                                                    std::size_t o = payloadBase + ii * 4u;
                                                    return (static_cast<std::uint32_t>(payloadData[o]) << 24) |
                                                           (static_cast<std::uint32_t>(payloadData[o + 1u]) << 16) |
                                                           (static_cast<std::uint32_t>(payloadData[o + 2u]) << 8) |
                                                           static_cast<std::uint32_t>(payloadData[o + 3u]);
                                                },
                                                0xFFFFFFFFu),
                                            "indirect_u32_strip");
                                    }

                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::Direct,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_lsb");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::Direct,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_msb");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::DirectZeroRestart,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_lsb_zr");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::DirectZeroRestart,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_msb_zr");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::OneBasedZeroRestart,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_lsb_ob");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::OneBasedZeroRestart,
                                            Packed10PrimitiveMode::Strip,
                                            payloadEntries),
                                        "indirect_p10_msb_ob");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::Direct,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_lsb_list");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::Direct,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_msb_list");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::DirectZeroRestart,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_lsb_zr_list");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::DirectZeroRestart,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_msb_zr_list");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            true,
                                            Packed10IndexStyle::OneBasedZeroRestart,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_lsb_ob_list");
                                    considerIndirectCandidate(
                                        decodePacked10FromPayload(
                                            false,
                                            Packed10IndexStyle::OneBasedZeroRestart,
                                            Packed10PrimitiveMode::List,
                                            payloadEntries),
                                        "indirect_p10_msb_ob_list");
                                };
                                decodeIndirectCommandPayload();

                                auto dropRatio = [](Ps3DecodeDropStats const& s) -> float {
                                    if (s.TokenTotal == 0u)
                                        return 0.0f;
                                    std::size_t dropped = s.Restart + s.OutOfRange + s.RemapDrop;
                                    return static_cast<float>(dropped) /
                                        static_cast<float>(s.TokenTotal);
                                };

                                auto registerDroppedStats = [&](Ps3DecodeDropStats const& s,
                                                                std::string_view modeLabel) {
                                    if (s.TokenTotal == 0u)
                                        return;
                                    ps3PackedDropTokenTotal += s.TokenTotal;
                                    ps3PackedDropRestartTotal += s.Restart;
                                    ps3PackedDropOutOfRangeTotal += s.OutOfRange;
                                    ps3PackedDropRemapTotal += s.RemapDrop;
                                    ps3PackedDropZeroIndexTotal += s.ZeroIndex;

                                    float ratio = dropRatio(s);
                                    if (ratio >= experimentalDroppedWarnRatio)
                                    {
                                        ++ps3PackedDropHeavyPrimitives;
                                        if (logPackedDroppedPrimitive)
                                        {
                                            std::cerr << "[PS3] dropped "
                                                      << target.Name << "_t" << treeIndex
                                                      << "_b" << branchIndex
                                                      << "_p" << primitiveIndex
                                                      << " mode=" << modeLabel
                                                      << " ratio=" << ratio
                                                      << " tokens=" << s.TokenTotal
                                                      << " restart=" << s.Restart
                                                      << " out=" << s.OutOfRange
                                                      << " remap=" << s.RemapDrop
                                                      << " zero=" << s.ZeroIndex
                                                      << '\n';
                                        }
                                    }
                                };

                                if (fallbackMode == 2 && !fallbackListIndices.empty())
                                {
                                    std::size_t stripTriangles = fallbackIndices.size() / 3u;
                                    std::size_t listTriangles = fallbackListIndices.size() / 3u;
                                    if (stripTriangles == 0u)
                                    {
                                        fallbackIndices = fallbackListIndices;
                                    }
                                    else
                                    {
                                        float stripScore = scoreIndexQuality(fallbackIndices);
                                        float listScore = scoreIndexQuality(fallbackListIndices);
                                        if (std::isfinite(listScore) &&
                                            listTriangles * 5u >= stripTriangles &&
                                            (listScore + 0.0015f < stripScore))
                                        {
                                            fallbackIndices = fallbackListIndices;
                                        }
                                    }
                                }

                                float fallbackScore = scoreIndexQuality(fallbackIndices);
                                float fallbackCoverage = indexCoverageRatio(fallbackIndices);
                                std::size_t fallbackTriangles = fallbackIndices.size() / 3u;
                                float bestScore = std::numeric_limits<float>::infinity();
                                std::size_t bestTriangles = 0u;
                                float bestCoverage = 0.0f;
                                enum class ChosenIndexKind
                                {
                                    Fallback,
                                    Indexed,
                                    Encoded,
                                    Packed10Bit,
                                    Indirect
                                };
                                ChosenIndexKind chosenKind = ChosenIndexKind::Fallback;
                                if (fallbackTriangles > 0u && fallbackMode != 4)
                                {
                                    bestScore = fallbackScore;
                                    bestTriangles = fallbackTriangles;
                                    bestCoverage = fallbackCoverage;
                                }

                                float indexedScoreCached = std::numeric_limits<float>::infinity();
                                std::size_t indexedTrianglesCached = 0u;
                                float indexedDropRatio = 1.0f;
                                if (indexedIndices.size() >= 3u)
                                {
                                    float indexedScore = scoreIndexQuality(indexedIndices);
                                    float indexedCoverage = indexCoverageRatio(indexedIndices);
                                    std::size_t indexedTriangles = indexedIndices.size() / 3u;
                                    indexedScoreCached = indexedScore;
                                    indexedTrianglesCached = indexedTriangles;
                                    indexedDropRatio = dropRatio(bestIndexed.Drops);
                                    bool acceptIndexed = std::isfinite(indexedScore);
                                    if (acceptIndexed && indexedDropRatio > 0.70f)
                                        acceptIndexed = false;
                                    if (acceptIndexed && fallbackTriangles > 0u)
                                    {
                                        bool coverageOk = indexedCoverage + 0.02f >= fallbackCoverage;
                                        bool scoreBetter = indexedScore + 0.001f < fallbackScore;
                                        bool trianglesReasonable = indexedTriangles * 4u >= fallbackTriangles;
                                        if (!coverageOk && !scoreBetter && !trianglesReasonable)
                                            acceptIndexed = false;
                                    }
                                    if (acceptIndexed &&
                                        vertexCount >= 256u &&
                                        indexedTriangles < 24u &&
                                        (indexedCoverage < 0.10f || indexedScore > 0.20f))
                                    {
                                        acceptIndexed = false;
                                    }
                                    float indexedAdjustedScore = indexedScore + indexedDropRatio * 0.50f;
                                    if (acceptIndexed &&
                                        (bestTriangles == 0u ||
                                         indexedAdjustedScore + 0.002f < bestScore ||
                                         (std::isfinite(bestScore) &&
                                          std::abs(indexedAdjustedScore - bestScore) <= 0.002f &&
                                          indexedTriangles > bestTriangles)))
                                    {
                                        bestScore = indexedAdjustedScore;
                                        bestTriangles = indexedTriangles;
                                        bestCoverage = indexedCoverage;
                                        chosenKind = ChosenIndexKind::Indexed;
                                    }
                                }

                                if (packed10BitIndices.size() >= 3u)
                                {
                                    std::size_t packed10BitTriangles = packed10BitIndices.size() / 3u;
                                    float packedCoverage = indexCoverageRatio(packed10BitIndices);
                                    std::size_t minPacked10Triangles = 24u;
                                    if (vertexCount <= 32u)
                                        minPacked10Triangles = 1u;
                                    else if (vertexCount <= 96u)
                                        minPacked10Triangles = 4u;
                                    else if (vertexCount <= 256u)
                                        minPacked10Triangles = 8u;
                                    bool acceptPacked10Bit =
                                        packed10BitTriangles >= minPacked10Triangles &&
                                        std::isfinite(packed10BitScore);
                                    if (acceptPacked10Bit && packed10BitDropRatio > 0.65f)
                                        acceptPacked10Bit = false;
                                    if (acceptPacked10Bit && packedCoverage < 0.05f)
                                        acceptPacked10Bit = false;
                                    if (acceptPacked10Bit && fallbackTriangles > 0u)
                                    {
                                        bool scoreBetter = (packed10BitScore + 0.001f) < fallbackScore;
                                        bool trianglesReasonable = (packed10BitTriangles * 3u) >= fallbackTriangles;
                                        bool coverageOk = packedCoverage + 0.04f >= fallbackCoverage;
                                        if (!scoreBetter && !trianglesReasonable && !coverageOk)
                                            acceptPacked10Bit = false;
                                    }
                                    if (acceptPacked10Bit && indexedTrianglesCached > 0u)
                                    {
                                        if (packed10BitDropRatio > indexedDropRatio + 0.08f &&
                                            packed10BitScore + 0.001f >= indexedScoreCached)
                                        {
                                            acceptPacked10Bit = false;
                                        }
                                        if (packed10BitTriangles * 10u < indexedTrianglesCached * 7u &&
                                            packed10BitScore > indexedScoreCached - 0.001f)
                                        {
                                            acceptPacked10Bit = false;
                                        }
                                    }
                                    float packed10BitAdjustedScore =
                                        packed10BitScore + packed10BitDropRatio * 0.75f;
                                    if (acceptPacked10Bit &&
                                        (bestTriangles == 0u ||
                                         packed10BitAdjustedScore + 0.002f < bestScore ||
                                         (std::isfinite(bestScore) &&
                                          std::abs(packed10BitAdjustedScore - bestScore) <= 0.002f &&
                                          packed10BitTriangles > bestTriangles)))
                                    {
                                        bestScore = packed10BitAdjustedScore;
                                        bestTriangles = packed10BitTriangles;
                                        bestCoverage = packedCoverage;
                                        chosenKind = ChosenIndexKind::Packed10Bit;
                                    }
                                }

                                if (indirectIndices.size() >= 3u)
                                {
                                    std::size_t indirectTriangles = indirectIndices.size() / 3u;
                                    float indirectCoverage = indexCoverageRatio(indirectIndices);
                                    std::size_t minIndirectTriangles = 24u;
                                    if (vertexCount <= 32u)
                                        minIndirectTriangles = 1u;
                                    else if (vertexCount <= 96u)
                                        minIndirectTriangles = 4u;
                                    else if (vertexCount <= 256u)
                                        minIndirectTriangles = 8u;
                                    bool acceptIndirect =
                                        indirectTriangles >= minIndirectTriangles &&
                                        std::isfinite(indirectScore);
                                    if (acceptIndirect && indirectCoverage < 0.05f)
                                        acceptIndirect = false;
                                    if (acceptIndirect && fallbackTriangles > 0u)
                                    {
                                        bool scoreBetter = indirectScore + 0.001f < fallbackScore;
                                        bool trianglesReasonable = indirectTriangles * 4u >= fallbackTriangles;
                                        bool coverageOk = indirectCoverage + 0.04f >= fallbackCoverage;
                                        if (!scoreBetter && !trianglesReasonable && !coverageOk)
                                            acceptIndirect = false;
                                    }
                                    if (acceptIndirect &&
                                        (bestTriangles == 0u ||
                                         indirectScore + 0.002f < bestScore ||
                                         (std::isfinite(bestScore) &&
                                          std::abs(indirectScore - bestScore) <= 0.002f &&
                                          indirectTriangles > bestTriangles)))
                                    {
                                        bestScore = indirectScore;
                                        bestTriangles = indirectTriangles;
                                        bestCoverage = indirectCoverage;
                                        chosenKind = ChosenIndexKind::Indirect;
                                    }
                                }

                                if (encodedIndices.size() >= 3u)
                                {
                                    std::size_t encodedTriangles = encodedIndices.size() / 3u;
                                    float encodedCandidateScore = scoreIndexQuality(encodedIndices);
                                    float encodedDominant = dominantIndexRatio(encodedIndices);
                                    float encodedCoverage = indexCoverageRatio(encodedIndices);
                                    std::size_t minimumEncodedTriangles =
                                        fallbackTriangles > 0u
                                        ? std::min<std::size_t>(24u, fallbackTriangles)
                                        : 24u;
                                    bool encodedBasicOk = std::isfinite(encodedCandidateScore) &&
                                        encodedDominant <= experimentalEncodedDominantMax &&
                                        encodedCoverage >= experimentalEncodedCoverageMin &&
                                        encodedTriangles >= minimumEncodedTriangles;
                                    if (encodedBasicOk && fallbackTriangles > 0u)
                                    {
                                        bool scoreBetter = encodedCandidateScore + 0.001f < fallbackScore;
                                        bool trianglesReasonable = encodedTriangles * 4u >= fallbackTriangles;
                                        bool coverageOk = encodedCoverage + 0.04f >= fallbackCoverage;
                                        if (!scoreBetter && !trianglesReasonable && !coverageOk)
                                            encodedBasicOk = false;
                                    }
                                    if (encodedBasicOk)
                                    {
                                        if (bestTriangles == 0u ||
                                            encodedCandidateScore + 0.002f < bestScore ||
                                            (std::isfinite(bestScore) &&
                                             std::abs(encodedCandidateScore - bestScore) <= 0.002f &&
                                             encodedTriangles > bestTriangles))
                                        {
                                            bestScore = encodedCandidateScore;
                                            bestTriangles = encodedTriangles;
                                            bestCoverage = encodedCoverage;
                                            chosenKind = ChosenIndexKind::Encoded;
                                        }
                                    }
                                }

                                if (bestTriangles == 0u && fallbackTriangles > 0u)
                                {
                                    bestScore = fallbackScore;
                                    bestTriangles = fallbackTriangles;
                                    bestCoverage = fallbackCoverage;
                                    chosenKind = ChosenIndexKind::Fallback;
                                }

                                // If native candidate is only slightly better but much sparser, fall back to denser option.
                                if (chosenKind != ChosenIndexKind::Fallback &&
                                    fallbackTriangles > 0u &&
                                    std::isfinite(bestScore) && std::isfinite(fallbackScore) &&
                                    std::abs(bestScore - fallbackScore) <= 0.0015f &&
                                    (fallbackCoverage + 0.02f > bestCoverage) &&
                                    fallbackTriangles * 20u > bestTriangles * 17u)
                                {
                                    bestScore = fallbackScore;
                                    bestTriangles = fallbackTriangles;
                                    bestCoverage = fallbackCoverage;
                                    chosenKind = ChosenIndexKind::Fallback;
                                }

                                std::vector<std::uint32_t> indices;
                                bool usedIndexedBuffer = false;
                                bool usedPacked10Bit = false;
                                bool usedIndirectPayload = false;
                                std::string_view droppedMode = "none";
                                Ps3DecodeDropStats droppedStats{};
                                if (chosenKind == ChosenIndexKind::Indexed)
                                {
                                    indices = std::move(indexedIndices);
                                    usedIndexedBuffer = true;
                                    droppedMode = indexedUsedList ? "direct_list" : "direct";
                                    droppedStats = bestIndexed.Drops;
                                }
                                else if (chosenKind == ChosenIndexKind::Packed10Bit)
                                {
                                    indices = std::move(packed10BitIndices);
                                    usedIndexedBuffer = true;
                                    usedPacked10Bit = true;
                                    if (packed10BitOneBased)
                                    {
                                        droppedMode = packed10BitList
                                            ? (packed10BitLsb ? "packed10_lsb_ob_list" : "packed10_msb_ob_list")
                                            : (packed10BitLsb ? "packed10_lsb_ob" : "packed10_msb_ob");
                                    }
                                    else if (packed10BitZeroRestart)
                                    {
                                        droppedMode = packed10BitList
                                            ? (packed10BitLsb ? "packed10_lsb_zr_list" : "packed10_msb_zr_list")
                                            : (packed10BitLsb ? "packed10_lsb_zr" : "packed10_msb_zr");
                                    }
                                    else
                                    {
                                        droppedMode = packed10BitList
                                            ? (packed10BitLsb ? "packed10_lsb_list" : "packed10_msb_list")
                                            : (packed10BitLsb ? "packed10_lsb" : "packed10_msb");
                                    }
                                    droppedStats = packed10BitDrops;
                                }
                                else if (chosenKind == ChosenIndexKind::Encoded)
                                {
                                    if (encodedMask == 0x03FFu)
                                        ++ps3PackedEncodedMask3ffCount;
                                    else if (encodedMask == 0x00FFu)
                                        ++ps3PackedEncodedMask0ffCount;
                                    indices = std::move(encodedIndices);
                                    usedIndexedBuffer = true;
                                    droppedMode = "encoded";
                                }
                                else if (chosenKind == ChosenIndexKind::Indirect)
                                {
                                    indices = std::move(indirectIndices);
                                    usedIndexedBuffer = true;
                                    usedIndirectPayload = true;
                                    droppedMode = "indirect";
                                }
                                else
                                {
                                    indices = std::move(fallbackIndices);
                                    droppedMode = "fallback";
                                }

                                if (indices.size() >= 3)
                                {
                                    MeshOutput out;
                                    out.Name = target.Name + "_t" + std::to_string(treeIndex) +
                                        "_b" + std::to_string(branchIndex) +
                                        "_p" + std::to_string(primitiveIndex);
                                    out.Vertices = std::move(vertices);
                                    out.Indices = std::move(indices);
                                    outputs.emplace_back(std::move(out));
                                    ++ps3PackedPrimitiveOutputs;
                                    registerDroppedStats(droppedStats, droppedMode);
                                    if (usedPacked10Bit)
                                    {
                                        ++ps3Packed10BitPrimitiveOutputs;
                                    }
                                    if (usedIndexedBuffer)
                                    {
                                        if (logPackedIndexedPrimitive)
                                        {
                                            MeshOutput const& emitted = outputs.back();
                                            std::cerr << "[PS3] packedIndexed "
                                                      << emitted.Name
                                                      << " tris=" << (emitted.Indices.size() / 3u)
                                                      << " verts=" << emitted.Vertices.size()
                                                      << " kind="
                                                      << (chosenKind == ChosenIndexKind::Indexed
                                                          ? (indexedUsedList ? "direct_list" : "direct")
                                                          : (chosenKind == ChosenIndexKind::Packed10Bit
                                                              ? (packed10BitOneBased
                                                                  ? (packed10BitList
                                                                      ? (packed10BitLsb ? "packed10_lsb_ob_list" : "packed10_msb_ob_list")
                                                                      : (packed10BitLsb ? "packed10_lsb_ob" : "packed10_msb_ob"))
                                                                  : (packed10BitList
                                                                      ? (packed10BitLsb ? "packed10_lsb_list" : "packed10_msb_list")
                                                                      : (packed10BitLsb ? "packed10_lsb" : "packed10_msb")))
                                                              : (chosenKind == ChosenIndexKind::Indirect
                                                                  ? indirectMode
                                                                  : "command")))
                                                      << '\n';
                                        }
                                        ++ps3PackedIndexedPrimitiveOutputs;
                                        if (usedIndirectPayload)
                                            ++ps3PackedIndirectPrimitiveOutputs;
                                        if (chosenKind == ChosenIndexKind::Indexed)
                                        {
                                            if (indexedBytesPerIndex == 1u)
                                                ++ps3PackedIndexedU8Count;
                                            else if (indexedBytesPerIndex == 2u)
                                                ++ps3PackedIndexedU16Count;
                                            else if (indexedBytesPerIndex == 4u)
                                                ++ps3PackedIndexedU32Count;
                                        }
                                    }
                                    continue;
                                }
                            }
                        }
                    }
                }

                if (experimentalFallbackMode == 4)
                    continue;

                std::uint32_t streamRel = 0;
                if (!ReadBEU32(cpuData, primitiveBase + 0x98, streamRel) || !inForest(streamRel, 0x20u))
                    continue;

                std::size_t streamBase = forestBase + static_cast<std::size_t>(streamRel);
                std::uint32_t strideU32 = 0;
                std::uint32_t streamData = 0;
                std::uint32_t vertexCount = 0;
                if (!ReadBEU32(cpuData, streamBase + 0x4, strideU32) ||
                    !ReadBEU32(cpuData, streamBase + 0x8, streamData) ||
                    !ReadBEU32(cpuData, streamBase + 0x10, vertexCount))
                {
                    continue;
                }

                if (strideU32 == 0 || strideU32 > 0x200u || vertexCount < 3u || vertexCount > 500000u)
                    continue;
                int stride = static_cast<int>(strideU32);
                std::size_t streamBytes = static_cast<std::size_t>(stride) * static_cast<std::size_t>(vertexCount);
                if (!inForest(streamData, streamBytes))
                    continue;

                std::array<int, 7> positionCandidates{0, 2, 4, 6, 8, 10, 12};
                std::uint32_t sampleCount = std::min<std::uint32_t>(vertexCount, 64u);
                int bestOffset = 0;
                std::uint32_t bestValid = 0;
                for (int candidate : positionCandidates)
                {
                    std::uint32_t valid = 0;
                    for (std::uint32_t vi = 0; vi < sampleCount; ++vi)
                    {
                        std::size_t posOffset = forestBase + static_cast<std::size_t>(streamData) +
                                                static_cast<std::size_t>(vi) * static_cast<std::size_t>(stride) +
                                                static_cast<std::size_t>(candidate);
                        Vector3 pos{};
                        if (!ReadBEFloat3(cpuData, posOffset, pos))
                            continue;
                        float maxAbs = std::max(std::abs(pos.X), std::max(std::abs(pos.Y), std::abs(pos.Z)));
                        if (std::isfinite(maxAbs) && maxAbs < kBroadBound)
                            ++valid;
                    }
                    if (valid > bestValid)
                    {
                        bestValid = valid;
                        bestOffset = candidate;
                    }
                }

                if (sampleCount == 0 || bestValid * 2u < sampleCount)
                    continue;

                std::vector<int> remap(static_cast<std::size_t>(vertexCount), -1);
                std::vector<ObjVertex> vertices;
                vertices.reserve(static_cast<std::size_t>(vertexCount));

                for (std::uint32_t vi = 0; vi < vertexCount; ++vi)
                {
                    std::size_t posOffset = forestBase + static_cast<std::size_t>(streamData) +
                                            static_cast<std::size_t>(vi) * static_cast<std::size_t>(stride) +
                                            static_cast<std::size_t>(bestOffset);
                    Vector3 localPos{};
                    if (!ReadBEFloat3(cpuData, posOffset, localPos))
                        continue;

                    Vector4 pos4{localPos.X, localPos.Y, localPos.Z, 1.0f};
                    Vector4 transformed = Transform(worldMatrix, pos4);
                    if (!std::isfinite(transformed.X) || !std::isfinite(transformed.Y) || !std::isfinite(transformed.Z))
                        continue;

                    ObjVertex v{};
                    v.Pos = {transformed.X, transformed.Y, transformed.Z};
                    v.Normal = {0.0f, 1.0f, 0.0f};
                    v.Uv = DecodePs3UvTail(cpuData,
                                           forestBase + static_cast<std::size_t>(streamData),
                                           vi,
                                           stride);

                    remap[static_cast<std::size_t>(vi)] = static_cast<int>(vertices.size());
                    vertices.push_back(v);
                }

                if (vertices.size() < 3)
                    continue;

                std::vector<std::uint32_t> indices;
                indices.reserve(static_cast<std::size_t>(vertexCount) * 3u);
                bool flip = false;
                for (std::uint32_t vi = 2; vi < vertexCount; ++vi)
                {
                    int i0 = remap[static_cast<std::size_t>(vi - 2)];
                    int i1 = remap[static_cast<std::size_t>(vi - 1)];
                    int i2 = remap[static_cast<std::size_t>(vi)];
                    if (i0 < 0 || i1 < 0 || i2 < 0)
                    {
                        flip = !flip;
                        continue;
                    }
                    if (i0 == i1 || i1 == i2 || i0 == i2)
                    {
                        flip = !flip;
                        continue;
                    }

                    if (flip)
                    {
                        indices.push_back(static_cast<std::uint32_t>(i1));
                        indices.push_back(static_cast<std::uint32_t>(i0));
                        indices.push_back(static_cast<std::uint32_t>(i2));
                    }
                    else
                    {
                        indices.push_back(static_cast<std::uint32_t>(i0));
                        indices.push_back(static_cast<std::uint32_t>(i1));
                        indices.push_back(static_cast<std::uint32_t>(i2));
                    }
                    flip = !flip;
                }

                if (indices.size() < 3)
                    continue;

                    MeshOutput out;
                    out.Name = target.Name + "_t" + std::to_string(treeIndex) +
                               "_b" + std::to_string(branchIndex) +
                               "_m" + std::to_string(meshRel) +
                               "_p" + std::to_string(primitiveIndex);
                    out.Vertices = std::move(vertices);
                    out.Indices = std::move(indices);
                    outputs.emplace_back(std::move(out));
                    ++ps3LegacyStreamOutputs;
                }
            }
        }
    }

    std::cerr << "[PS3] decode stats: tableEntries=" << streamTableChoice.Entries.size()
              << " tableScore=" << streamTableChoice.Score
              << " slot=" << ps3SlotPrimitiveOutputs
              << " slotRef=" << ps3StreamSlotReferenced
              << " slotRes=" << ps3StreamSlotResolved
              << " slotOob=" << ps3StreamSlotOob
              << " packed=" << ps3PackedPrimitiveOutputs
              << " packedIndexed=" << ps3PackedIndexedPrimitiveOutputs
              << " packedIdxU8=" << ps3PackedIndexedU8Count
              << " packedIdxU16=" << ps3PackedIndexedU16Count
              << " packedIdxU32=" << ps3PackedIndexedU32Count
              << " packed10=" << ps3Packed10BitPrimitiveOutputs
              << " packedIndirect=" << ps3PackedIndirectPrimitiveOutputs
              << " packedEnc3ff=" << ps3PackedEncodedMask3ffCount
              << " packedEnc0ff=" << ps3PackedEncodedMask0ffCount
              << " packedExtra78=" << ps3PackedExtra78Count
              << " packedExtra80=" << ps3PackedExtra80Count
              << " packedExtra88=" << ps3PackedExtra88Count
              << " packedMulti=" << ps3PackedMultiCount
              << " dropTok=" << ps3PackedDropTokenTotal
              << " dropRestart=" << ps3PackedDropRestartTotal
              << " dropOut=" << ps3PackedDropOutOfRangeTotal
              << " dropRemap=" << ps3PackedDropRemapTotal
              << " dropZero=" << ps3PackedDropZeroIndexTotal
              << " dropHeavy=" << ps3PackedDropHeavyPrimitives
              << " stream=" << ps3LegacyStreamOutputs
              << '\n';

    return !outputs.empty();
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "forest_to_obj <track.Forest|track.sif> <output.obj>\n";
        return 1;
    }

    std::filesystem::path inputPath(argv[1]);
    std::filesystem::path outputPath(argv[2]);

    std::ifstream input(inputPath, std::ios::binary);
    if (!input)
    {
        std::cerr << "Unable to open " << inputPath << '\n';
        return 2;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(input)), {});
    if (buffer.empty())
    {
        std::cerr << "Empty forest payload.\n";
        return 3;
    }

    std::vector<std::uint8_t> rawData(buffer.begin(), buffer.end());
    std::vector<std::uint8_t> cpuData;
    std::vector<std::uint32_t> relocationOffsets;
    std::vector<std::uint8_t> gpuData;
    bool bigEndian = false;
    std::vector<MeshOutput> outputs;
    if (!TryParseForestArchive(std::span<const std::uint8_t>(rawData.data(), rawData.size()),
                               cpuData,
                               relocationOffsets,
                               gpuData,
                               bigEndian))
    {
        std::string extension = ToLower(inputPath.extension().string());
        if (extension == ".sif")
        {
            std::string sifError;
            if (!TryExtractForestFromSif(inputPath,
                                         std::span<const std::uint8_t>(rawData.data(), rawData.size()),
                                         cpuData,
                                         relocationOffsets,
                                         gpuData,
                                         bigEndian,
                                         sifError))
            {
                std::cerr << "Failed to parse SIF forest data: " << sifError << '\n';
                return 4;
            }
        }
        else
        {
            cpuData = std::move(rawData);
            relocationOffsets.clear();
            gpuData.clear();
            bigEndian = false;
        }
    }

    bool ps3ByName = IsLikelyPs3Path(inputPath);
    bool enablePs3TriangleFilter = true;
    if (char const* envFilter = std::getenv("FOREST_TO_OBJ_PS3_FILTER_STRETCHED"))
        enablePs3TriangleFilter = (envFilter[0] == '1');
    if (bigEndian || ps3ByName)
    {
        std::span<const std::uint8_t> cpuSpan = cpuData.empty()
            ? std::span<const std::uint8_t>()
            : std::span<const std::uint8_t>(cpuData.data(), cpuData.size());
        std::span<const std::uint8_t> gpuSpan = gpuData.empty()
            ? std::span<const std::uint8_t>()
            : std::span<const std::uint8_t>(gpuData.data(), gpuData.size());

        bool trackOnly = false;
        if (char const* envTrackOnly = std::getenv("FOREST_TO_OBJ_PS3_TRACK_ONLY"))
            trackOnly = (envTrackOnly[0] == '1');
        bool decodeAllPs3 = true;
        if (char const* envDecodeAll = std::getenv("FOREST_TO_OBJ_PS3_DECODE_ALL"))
            decodeAllPs3 = (envDecodeAll[0] != '0');

        std::vector<std::string_view> ps3ForestOrder;
        if (trackOnly)
        {
            ps3ForestOrder = {"track.forest"};
        }
        else
        {
            ps3ForestOrder = {"track.forest", "item.forest", "track_shadow.forest"};
            if (char const* allForests = std::getenv("FOREST_TO_OBJ_PS3_ALL_FORESTS"))
            {
                if (allForests[0] == '0')
                    ps3ForestOrder = {"track.forest"};
            }
        }

        std::vector<MeshOutput> decoded;
        if (decodeAllPs3)
        {
            if (TryDecodePs3TrackMeshes(cpuSpan, gpuSpan, decoded, "track.forest", true))
            {
                for (auto& mesh : decoded)
                    outputs.emplace_back(std::move(mesh));
            }
        }
        else
        {
            for (std::string_view forestName : ps3ForestOrder)
            {
                if (!TryDecodePs3TrackMeshes(cpuSpan, gpuSpan, decoded, forestName, false))
                    continue;
                for (auto& mesh : decoded)
                    outputs.emplace_back(std::move(mesh));
            }
        }
    }

    if (outputs.empty())
    {
        std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
        relocations.reserve(relocationOffsets.size());
        for (auto offset : relocationOffsets)
            relocations.push_back({static_cast<int>(offset), 0});

        SlLib::Serialization::ResourceLoadContext context(
            cpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(cpuData.data(), cpuData.size()),
            gpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(gpuData.data(), gpuData.size()),
            std::move(relocations));
        SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
        SlLib::Resources::Database::SlPlatform ps3("ps3", true, false, 0);
        context.Platform = bigEndian ? &ps3 : &win32;
        context.Version = 0;

        SeEditor::Forest::ForestLibrary library;
        try
        {
            library.Load(context);
        }
        catch (std::exception const& e)
        {
            std::cerr << "Failed to load forest library: " << e.what() << '\n';
            return 4;
        }

    auto appendMesh = [&](std::shared_ptr<SeEditor::Forest::SuRenderMesh> const& mesh,
                          Matrix4x4 const& worldMatrix,
                          std::string const& sourceName) {
        if (!mesh)
            return;
        for (std::size_t primIdx = 0; primIdx < mesh->Primitives.size(); ++primIdx)
        {
            auto const& primitive = mesh->Primitives[primIdx];
            if (!primitive || !primitive->VertexStream)
                continue;

            auto verts = DecodeVertex(*primitive->VertexStream);
            if (verts.empty())
                continue;

            Matrix4x4 normalMatrix = worldMatrix;
            normalMatrix(0, 3) = 0.0f;
            normalMatrix(1, 3) = 0.0f;
            normalMatrix(2, 3) = 0.0f;

            for (auto& v : verts)
            {
                Vector4 pos4{v.Pos.X, v.Pos.Y, v.Pos.Z, 1.0f};
                auto transformed = Transform(worldMatrix, pos4);
                v.Pos = {transformed.X, transformed.Y, transformed.Z};

                Vector4 n4{v.Normal.X, v.Normal.Y, v.Normal.Z, 0.0f};
                auto nT = Transform(normalMatrix, n4);
                Vector3 norm{nT.X, nT.Y, nT.Z};
                float length = std::sqrt(norm.X * norm.X + norm.Y * norm.Y + norm.Z * norm.Z);
                if (length > 0.0f)
                    v.Normal = {norm.X / length, norm.Y / length, norm.Z / length};
                else
                    v.Normal = {0.0f, 1.0f, 0.0f};
            }

            std::size_t indexCount = primitive->IndexData.size() / 2;
            if (primitive->NumIndices > 0)
                indexCount = std::min<std::size_t>(indexCount, static_cast<std::size_t>(primitive->NumIndices));
            else if (indexCount == 0)
                continue;

            std::size_t vertexLimit = verts.size();
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
                    std::uint16_t a = primitive->IndexData[i * 2];
                    std::uint16_t b = primitive->IndexData[i * 2 + 1];
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

            bool use32Bit = best.Use32;
            bool swapIndices = best.Swap;
            std::vector<std::uint32_t> rawIndices;
            rawIndices.reserve(best.Count);
            if (use32Bit)
            {
                for (std::size_t i = 0; i < best.Count; ++i)
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
                    rawIndices.push_back(idx);
                }
            }

            int primitiveType = primitive->Unknown_0x9c;
            bool isStrip = primitiveType == 5 || (primitiveType != 4 && best.Restart > 0);
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

            if (validIndices.empty())
                continue;

            MeshOutput meshOut;
            meshOut.Name = sourceName + "_prim" + std::to_string(primIdx);
            if (primitive->Material && !primitive->Material->Name.empty())
                meshOut.MaterialName = primitive->Material->Name;
            else
                meshOut.MaterialName = meshOut.Name;
            meshOut.Material = primitive->Material;
            meshOut.Vertices = std::move(verts);
            meshOut.Indices = std::move(validIndices);
            outputs.emplace_back(std::move(meshOut));
        }
    };

    for (std::size_t forestIdx = 0; forestIdx < library.Forests.size(); ++forestIdx)
    {
        auto const& forestEntry = library.Forests[forestIdx];
        if (!forestEntry.Forest)
            continue;
        auto const& trees = forestEntry.Forest->Trees;
        for (std::size_t treeIdx = 0; treeIdx < trees.size(); ++treeIdx)
        {
            auto const& tree = trees[treeIdx];
            if (!tree)
                continue;

            std::size_t branchCount = tree->Branches.size();
            std::vector<Matrix4x4> world(branchCount);
            std::vector<bool> computed(branchCount, false);
            auto computeWorld = [&](auto&& self, int idx) -> Matrix4x4 {
                if (idx < 0 || static_cast<std::size_t>(idx) >= branchCount)
                    return Matrix4x4{};
                if (computed[static_cast<std::size_t>(idx)])
                    return world[static_cast<std::size_t>(idx)];

                Vector4 t{};
                Vector4 r{};
                Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
                if (static_cast<std::size_t>(idx) < tree->Translations.size())
                    t = tree->Translations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Rotations.size())
                    r = tree->Rotations[static_cast<std::size_t>(idx)];
                if (static_cast<std::size_t>(idx) < tree->Scales.size())
                    s = tree->Scales[static_cast<std::size_t>(idx)];

                Matrix4x4 local = BuildLocalMatrix(t, r, s);
                int parentIndex = tree->Branches[static_cast<std::size_t>(idx)]->Parent;
                if (parentIndex >= 0 && parentIndex < static_cast<int>(branchCount))
                {
                    world[static_cast<std::size_t>(idx)] =
                        Multiply(self(self, parentIndex), local);
                }
                else
                {
                    world[static_cast<std::size_t>(idx)] = local;
                }

                computed[static_cast<std::size_t>(idx)] = true;
                return world[static_cast<std::size_t>(idx)];
            };

            std::size_t treeMeshStart = outputs.size();
            for (std::size_t branchIdx = 0; branchIdx < branchCount; ++branchIdx)
            {
                Matrix4x4 worldMatrix = computeWorld(computeWorld, static_cast<int>(branchIdx));
                auto const& branch = tree->Branches[branchIdx];
                if (!branch)
                    continue;

                if (branch->Mesh)
                    appendMesh(branch->Mesh, worldMatrix,
                               forestEntry.Name.empty() ? ("forest" + std::to_string(forestIdx))
                                                        : forestEntry.Name);
                if (branch->Lod)
                {
                    for (auto const& threshold : branch->Lod->Thresholds)
                    {
                        if (threshold && threshold->Mesh)
                            appendMesh(threshold->Mesh, worldMatrix,
                                       forestEntry.Name.empty() ? ("forest" + std::to_string(forestIdx))
                                                                : forestEntry.Name);
                    }
                }
            }
        }
    }
    }

    if ((bigEndian || ps3ByName) && enablePs3TriangleFilter && !outputs.empty())
    {
        std::size_t removedTriangles = 0;
        for (auto& mesh : outputs)
            removedTriangles += FilterPs3Triangles(mesh);
        outputs.erase(std::remove_if(outputs.begin(), outputs.end(),
                                     [](MeshOutput const& mesh) { return mesh.Indices.size() < 3u; }),
                      outputs.end());
        if (removedTriangles > 0)
            std::cerr << "[PS3] filtered stretched triangles: " << removedTriangles << '\n';
    }

    for (auto& mesh : outputs)
        CompactMesh(mesh);

    if (outputs.empty())
    {
        std::cerr << "No mesh data generated.\n";
        return 5;
    }

    std::unordered_map<std::string, std::vector<std::uint8_t>> pcTextureFallback;
    if (bigEndian || ps3ByName)
        pcTextureFallback = LoadPcTextureFallback(inputPath);

    std::unordered_map<void*, std::string> materialTextureCache;
    std::unordered_map<std::string, std::string> writtenTextures;
    int anonTextureCounter = 0;

    auto lowerName = [](std::string s) {
        for (char& c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };

    auto writeTextureForMaterial = [&](std::shared_ptr<SeEditor::Forest::SuRenderMaterial> const& mat) -> std::string {
        if (!mat)
            return {};
        auto itCache = materialTextureCache.find(mat.get());
        if (itCache != materialTextureCache.end())
            return itCache->second;

        auto pickName = [&](std::string const& base) {
            if (!base.empty())
                return base;
            return std::string("tex_") + std::to_string(++anonTextureCounter) + ".dds";
        };

        for (auto const& tex : mat->Textures)
        {
            if (!tex || !tex->TextureResource)
                continue;
            auto const& res = tex->TextureResource;
            std::string fname = pickName(std::filesystem::path(res->Name).filename().string());
            if (std::filesystem::path(fname).extension().empty())
                fname += ".dds";
            std::string key = lowerName(fname);

            std::vector<std::uint8_t> bytes;
            if (!res->ImageData.empty())
                bytes = res->ImageData;
            else
            {
                auto it = pcTextureFallback.find(key);
                if (it != pcTextureFallback.end())
                    bytes = it->second;
            }
            if (bytes.empty())
                continue;

            std::filesystem::path outTex = outputPath.parent_path() / fname;
            if (writtenTextures.find(key) == writtenTextures.end())
            {
                std::ofstream texOut(outTex, std::ios::binary);
                if (texOut)
                {
                    texOut.write(reinterpret_cast<char const*>(bytes.data()),
                                 static_cast<std::streamsize>(bytes.size()));
                    writtenTextures.emplace(key, fname);
                }
            }
            materialTextureCache.emplace(mat.get(), fname);
            return fname;
        }

        materialTextureCache.emplace(mat.get(), std::string{});
        return {};
    };

    std::ofstream obj(outputPath);
    if (!obj)
    {
        std::cerr << "Failed to open " << outputPath << " for writing.\n";
        return 6;
    }

    std::filesystem::path mtlPath = outputPath;
    mtlPath.replace_extension(".mtl");

    std::ofstream mtl(mtlPath);
    if (!mtl)
    {
        std::cerr << "Failed to open " << mtlPath << " for writing.\n";
        return 6;
    }

    obj << "mtllib " << mtlPath.filename().string() << '\n';

    std::size_t vertexBase = 1;
    for (auto const& mesh : outputs)
    {
        obj << "o " << SanitizeName(mesh.Name) << '\n';
        std::string matName = SanitizeName(mesh.MaterialName.empty() ? mesh.Name : mesh.MaterialName);
        obj << "usemtl " << matName << '\n';
        mtl << "newmtl " << matName << '\n';
        mtl << "Ka 1.0 1.0 1.0\n";
        mtl << "Kd 1.0 1.0 1.0\n";
        mtl << "Ks 0.0 0.0 0.0\n";
        mtl << "d 1.0\n";
        mtl << "illum 1\n";
        std::string texFile = writeTextureForMaterial(mesh.Material);
        if (!texFile.empty())
            mtl << "map_Kd ./"
                << std::filesystem::path(texFile).filename().string()
                << "\n\n";
        else
            mtl << "\n";
        for (auto const& v : mesh.Vertices)
            obj << "v " << v.Pos.X << ' ' << v.Pos.Y << ' ' << v.Pos.Z << '\n';
        for (auto const& v : mesh.Vertices)
            obj << "vn " << v.Normal.X << ' ' << v.Normal.Y << ' ' << v.Normal.Z << '\n';
        for (auto const& v : mesh.Vertices)
            obj << "vt " << v.Uv.X << ' ' << v.Uv.Y << '\n';

        for (std::size_t i = 0; i + 2 < mesh.Indices.size(); i += 3)
        {
            obj << "f ";
            for (int v = 0; v < 3; ++v)
            {
                std::size_t idx = mesh.Indices[i + v];
                obj << vertexBase + idx << '/' << vertexBase + idx << '/' << vertexBase + idx;
                if (v < 2)
                    obj << ' ';
            }
            obj << '\n';
        }

        vertexBase += mesh.Vertices.size();
    }

    std::cout << "Exported " << outputs.size() << " mesh chunks to " << outputPath << ".\n";
    return 0;
}
