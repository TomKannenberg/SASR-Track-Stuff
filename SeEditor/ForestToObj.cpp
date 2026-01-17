#include "SeEditor/Forest/ForestArchive.hpp"
#include "Forest/ForestTypes.hpp"
#include "SifParser.hpp"

#include <SlLib/Math/Vector.hpp>
#include <SlLib/Resources/Database/SlPlatform.hpp>
#include <SlLib/Resources/Database/SlResourceRelocation.hpp>
#include <SlLib/Serialization/ResourceLoadContext.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <string>
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
    std::vector<ObjVertex> Vertices;
    std::vector<std::uint32_t> Indices;
};

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

} // namespace

int main(int argc, char** argv)
{
    if (argc != 3)
    {
        std::cout << "forest_to_obj <track.Forest> <output.obj>\n";
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
    if (!TryParseForestArchive(std::span<const std::uint8_t>(rawData.data(), rawData.size()),
                               cpuData,
                               relocationOffsets,
                               gpuData,
                               bigEndian))
    {
        cpuData = std::move(rawData);
        relocationOffsets.clear();
        gpuData.clear();
        bigEndian = false;
    }

    std::vector<SlLib::Resources::Database::SlResourceRelocation> relocations;
    relocations.reserve(relocationOffsets.size());
    for (auto offset : relocationOffsets)
        relocations.push_back({static_cast<int>(offset), 0});

    SlLib::Serialization::ResourceLoadContext context(
        cpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(cpuData.data(), cpuData.size()),
        gpuData.empty() ? std::span<const std::uint8_t>() : std::span<const std::uint8_t>(gpuData.data(), gpuData.size()),
        std::move(relocations));
    SlLib::Resources::Database::SlPlatform win32("win32", false, false, 0);
    SlLib::Resources::Database::SlPlatform xbox360("x360", true, false, 0);
    context.Platform = bigEndian ? &xbox360 : &win32;
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

    std::vector<MeshOutput> outputs;
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

    if (outputs.empty())
    {
        std::cerr << "No mesh data generated.\n";
        return 5;
    }

    std::ofstream obj(outputPath);
    if (!obj)
    {
        std::cerr << "Failed to open " << outputPath << " for writing.\n";
        return 6;
    }

    std::size_t vertexBase = 1;
    for (auto const& mesh : outputs)
    {
        obj << "o " << SanitizeName(mesh.Name) << '\n';
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
