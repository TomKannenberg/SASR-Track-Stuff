
#include "ForestTypes.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Utilities/DdsUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace SeEditor::Forest {

namespace {
constexpr int kMaxForestCount = 1'000'000;
constexpr int kMaxStreams = 2;

int ClampCount(int count, char const* label)
{
    if (count < 0 || count > kMaxForestCount)
    {
        std::cerr << "[Forest] " << label << " count out of range: " << count << std::endl;
        return 0;
    }
    return count;
}

bool IsPointerRangeValid(std::span<const std::uint8_t> data, int address, std::size_t length)
{
    if (address < 0)
        return false;

    std::size_t start = static_cast<std::size_t>(address);
    if (start > data.size())
        return false;

    return data.size() - start >= length;
}

float DenormalizeSigned10BitInt(std::uint16_t value)
{
    int signedValue = static_cast<int>(value & 0x3FF);
    if (signedValue & 0x200)
        signedValue |= ~0x3FF;
    float maxValue = 511.0f;
    return static_cast<float>(signedValue) / maxValue;
}

float DenormalizeUnsigned3BitInt(std::uint8_t value)
{
    return static_cast<float>(value & 0x7) / 7.0f;
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
        return sign ? -std::ldexp(m, -14) : std::ldexp(m, -14);
    }
    if (exp == 31)
        return mant ? std::numeric_limits<float>::quiet_NaN()
                    : (sign ? -std::numeric_limits<float>::infinity()
                            : std::numeric_limits<float>::infinity());

    float m = 1.0f + mant / 1024.0f;
    return sign ? -std::ldexp(m, exp - 15) : std::ldexp(m, exp - 15);
}

std::uint16_t FloatToHalf(float value)
{
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    std::uint32_t sign = (bits >> 31) & 0x1;
    std::uint32_t exp = (bits >> 23) & 0xFF;
    std::uint32_t mant = bits & 0x7FFFFF;

    std::uint16_t outSign = static_cast<std::uint16_t>(sign << 15);
    if (exp == 255)
    {
        std::uint16_t outExp = 0x1F << 10;
        std::uint16_t outMant = mant ? 0x200 : 0;
        return static_cast<std::uint16_t>(outSign | outExp | outMant);
    }

    int newExp = static_cast<int>(exp) - 127 + 15;
    if (newExp >= 31)
        return static_cast<std::uint16_t>(outSign | (0x1F << 10));
    if (newExp <= 0)
    {
        if (newExp < -10)
            return outSign;
        mant |= 0x800000;
        int shift = 14 - newExp;
        std::uint16_t outMant = static_cast<std::uint16_t>(mant >> shift);
        return static_cast<std::uint16_t>(outSign | outMant);
    }

    std::uint16_t outExp = static_cast<std::uint16_t>(newExp << 10);
    std::uint16_t outMant = static_cast<std::uint16_t>(mant >> 13);
    return static_cast<std::uint16_t>(outSign | outExp | outMant);
}
} // namespace

int D3DVertexElement::GetTypeSize(D3DDeclType type)
{
    switch (type)
    {
    case D3DDeclType::Float2: return 0x8;
    case D3DDeclType::Float3: return 0xC;
    case D3DDeclType::Float4: return 0x10;
    case D3DDeclType::Short4: return 0x8;
    case D3DDeclType::Short4N: return 0x8;
    case D3DDeclType::UShort4N: return 0x8;
    case D3DDeclType::Float16x4: return 0x8;
    default: return 0x4;
    }
}

void SuNameFloatPairs::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int numPairs = ClampCount(context.ReadInt32(), "FloatPairs");
    if (numPairs == 0)
        return;
    Pairs.clear();
    Pairs.reserve(static_cast<std::size_t>(numPairs));
    for (int i = 0; i < numPairs; ++i)
        Pairs.emplace_back(context.ReadInt32(), context.ReadFloat());
}

int XboxVertexElement::GetTypeSize(XboxDeclType type)
{
    switch (type)
    {
    case XboxDeclType::Float2:
    case XboxDeclType::Int2:
    case XboxDeclType::Int2N:
    case XboxDeclType::UInt2:
    case XboxDeclType::UInt2N:
        return 0x8;
    case XboxDeclType::Float3:
        return 0xC;
    case XboxDeclType::Float4:
    case XboxDeclType::Int4:
    case XboxDeclType::Int4N:
    case XboxDeclType::UInt4:
    case XboxDeclType::UInt4N:
        return 0x10;
    case XboxDeclType::Short4:
    case XboxDeclType::Short4N:
    case XboxDeclType::UShort4:
    case XboxDeclType::UShort4N:
    case XboxDeclType::Float16x4:
        return 0x8;
    default:
        return 0x4;
    }
}

D3DDeclType XboxVertexElement::MapTypeToD3D(XboxDeclType type)
{
    switch (type)
    {
    case XboxDeclType::Float1: return D3DDeclType::Float1;
    case XboxDeclType::Float2: return D3DDeclType::Float2;
    case XboxDeclType::Float3: return D3DDeclType::Float3;
    case XboxDeclType::Float4: return D3DDeclType::Float4;
    case XboxDeclType::D3DColor: return D3DDeclType::D3DColor;
    case XboxDeclType::UByte4: return D3DDeclType::UByte4;
    case XboxDeclType::Short2: return D3DDeclType::Short2;
    case XboxDeclType::Short4: return D3DDeclType::Short4;
    case XboxDeclType::UByte4N: return D3DDeclType::UByte4N;
    case XboxDeclType::Byte4N: return D3DDeclType::UByte4N;
    case XboxDeclType::Short2N: return D3DDeclType::Short2N;
    case XboxDeclType::Short4N: return D3DDeclType::Short4N;
    case XboxDeclType::UShort2N: return D3DDeclType::UShort2N;
    case XboxDeclType::UShort4N: return D3DDeclType::UShort4N;
    case XboxDeclType::UDec3: return D3DDeclType::UDec3;
    case XboxDeclType::Dec3N: return D3DDeclType::Dec3N;
    case XboxDeclType::Float16x2: return D3DDeclType::Float16x2;
    case XboxDeclType::Float16x4: return D3DDeclType::Float16x4;
    default:
        return D3DDeclType::Float4;
    }
}

int SuNameFloatPairs::GetSizeForSerialization() const
{
    return static_cast<int>(0x4 + Pairs.size() * 0x8);
}

void SuNameUint32Pairs::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int numPairs = ClampCount(context.ReadInt32(), "Uint32Pairs");
    if (numPairs == 0)
        return;
    Pairs.clear();
    Pairs.reserve(static_cast<std::size_t>(numPairs));
    for (int i = 0; i < numPairs; ++i)
        Pairs.emplace_back(context.ReadInt32(), context.ReadInt32());
}

int SuNameUint32Pairs::GetSizeForSerialization() const
{
    return static_cast<int>(0x4 + Pairs.size() * 0x8);
}

void MaximalTreeCollisionOOBB::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    ObbTransform = context.ReadMatrix(context.Position);
    context.Position += 0x40;
    Extents = context.ReadFloat4();
}

int MaximalTreeCollisionOOBB::GetSizeForSerialization() const
{
    return 0x50;
}

void SuBlindData::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int numElements = ClampCount(context.ReadInt32(), "BlindData");
    if (numElements == 0)
        return;
    Elements.clear();
    Elements.reserve(static_cast<std::size_t>(numElements));
    for (int i = 0; i < numElements; ++i)
    {
        Element element;
        element.InstanceHash = context.ReadInt32();
        element.TypeHash = context.ReadInt32();
        if (element.TypeHash == MaximalTreeCollisionOobbTypeHash)
            element.Data = context.LoadSharedPointer<MaximalTreeCollisionOOBB>();
        else if (element.InstanceHash == FloatPairsInstanceHash)
            element.Data = context.LoadSharedPointer<SuNameFloatPairs>();
        else if (element.InstanceHash == UintPairsInstanceHash)
            element.Data = context.LoadSharedPointer<SuNameUint32Pairs>();
        Elements.push_back(std::move(element));
    }
}

int SuBlindData::GetSizeForSerialization() const
{
    return static_cast<int>(0x4 + Elements.size() * 0xC);
}

void SuRenderTextureResource::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Name = context.ReadStringPointer();
    context.ReadInt32();
    int imageData = context.ReadPointer();
    context.ReadInt32();

    ImageData.clear();
    if (imageData == 0)
        return;

    auto span = context.LoadBuffer(imageData, 0x80, true);
    if (span.empty())
        span = context.LoadBuffer(imageData, 0x80, false);

    SlLib::Utilities::DdsInfo info;
    std::string error;
    if (!SlLib::Utilities::DdsUtil::Parse(span, info, error))
        return;

    std::size_t totalSize = SlLib::Utilities::DdsUtil::ComputeTotalSize(info);
    auto full = context.LoadBuffer(imageData, static_cast<int>(totalSize), true);
    if (full.empty())
        full = context.LoadBuffer(imageData, static_cast<int>(totalSize), false);

    ImageData.assign(full.begin(), full.end());
}

int SuRenderTextureResource::GetSizeForSerialization() const
{
    return 0x10;
}

void SuRenderTexture::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Flags = context.ReadInt32();
    TextureResource = context.LoadSharedPointer<SuRenderTextureResource>();
    Param0 = context.ReadInt32();
    Param1 = context.ReadInt32();
    Param2 = context.ReadInt32();
}

int SuRenderTexture::GetSizeForSerialization() const
{
    return 0x14;
}

void SuRenderMaterial::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    context.ReadInt32();
    PixelShaderFlags = context.ReadInt32();
    Hash = context.ReadInt32();
    int floatCount = context.ReadInt32();
    FloatList = context.LoadArrayPointer(floatCount, &SlLib::Serialization::ResourceLoadContext::ReadFloat);

    std::array<int, 6> layerCounts{};
    for (int i = 0; i < 6; ++i)
        layerCounts[i] = context.ReadInt32();

    for (int i = 0; i < 6; ++i)
        Layers[i] = context.LoadArrayPointer(layerCounts[i], &SlLib::Serialization::ResourceLoadContext::ReadInt32);

    Unknown_0x44 = context.ReadInt32();
    Unknown_0x48 = context.ReadInt32();
    Unknown_0x4c = context.ReadInt32();

    context.ReadInt8();
    Unknown_0x51 = static_cast<std::uint8_t>(context.ReadInt8());
    context.ReadInt16();

    Unknown_0x54 = context.ReadInt32();
    context.ReadInt32();

    int numElements = context.ReadInt32();
    int elementData = context.ReadPointer();
    if (numElements > 0 && elementData != 0)
    {
        int elementCount = context.ReadInt32(static_cast<std::size_t>(elementData));
        if (elementCount == 1)
        {
            int value = context.ReadInt32(static_cast<std::size_t>(elementData + 4));
            UnknownNumberList.push_back(value);
        }
    }

    Unknown_0x64 = context.ReadInt32();
    Unknown_0x68 = static_cast<std::uint8_t>(context.ReadInt8());
    Unknown_0x69 = static_cast<std::uint8_t>(context.ReadInt8());
    Unknown_0x6a = static_cast<std::uint8_t>(context.ReadInt8());
    Unknown_0x6b = static_cast<std::uint8_t>(context.ReadInt8());
    context.ReadInt32();
    context.ReadInt32();

    int textureCount = context.ReadInt32();
    Textures = context.LoadPointerArray<SuRenderTexture>(textureCount);
    Name = context.ReadStringPointer();
}

int SuRenderMaterial::GetSizeForSerialization() const
{
    return 0x80;
}
void SuRenderVertexStream::VertexStreamHashes::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Flags = context.ReadInt32();
}

int SuRenderVertexStream::VertexStreamHashes::GetSizeForSerialization() const
{
    return 0x4 + (NumStreams * 0x4);
}

void SuRenderVertexStream::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int attributeData = context.ReadPointer();
    auto const cpuSpan = context.Data();
    if (attributeData == 0 || !IsPointerRangeValid(cpuSpan, attributeData, 2))
    {
        std::cerr << "[Forest] Vertex stream attribute table out of range: " << attributeData << std::endl;
        return;
    }

    AttributeStreamsInfo.clear();
    std::array<int, kMaxStreams> streamSizes{0, 0};

    if (context.Platform && context.Platform->IsBigEndian())
    {
        std::vector<XboxVertexElement> xboxElements;
        int mainStreamSize = 0;
        std::size_t offset = static_cast<std::size_t>(attributeData);

        while (IsPointerRangeValid(cpuSpan, static_cast<int>(offset), 2))
        {
            if (context.ReadInt16(offset) == static_cast<std::int16_t>(0xFF))
                break;

            if (!IsPointerRangeValid(cpuSpan, static_cast<int>(offset), 12))
            {
                std::cerr << "[Forest] Xbox vertex attribute descriptor truncated near offset: " << offset << std::endl;
                break;
            }

            XboxVertexElement xboxElement;
            xboxElement.Stream = context.ReadInt16(offset + 0);
            xboxElement.Offset = context.ReadInt16(offset + 2);
            xboxElement.Type = static_cast<XboxDeclType>(context.ReadInt32(offset + 4));
            xboxElement.Method = static_cast<D3DDeclMethod>(context.ReadInt8(offset + 8));
            xboxElement.Usage = static_cast<D3DDeclUsage>(context.ReadInt8(offset + 9));
            xboxElement.UsageIndex = static_cast<std::uint8_t>(context.ReadInt8(offset + 10));

            xboxElements.push_back(xboxElement);
            if (xboxElement.Stream >= kMaxStreams)
            {
                std::cerr << "[Forest] Xbox vertex stream count exceeds limit." << std::endl;
                break;
            }

            XboxDeclType mappedType = xboxElement.Type;
            if (mappedType == XboxDeclType::Dec3N || mappedType == XboxDeclType::Dec4N)
                mappedType = XboxDeclType::Float16x4;

            int size = xboxElement.Offset + XboxVertexElement::GetTypeSize(xboxElement.Type);
            streamSizes[static_cast<std::size_t>(xboxElement.Stream)] =
                std::max(streamSizes[static_cast<std::size_t>(xboxElement.Stream)], size);

            int elementOffset = xboxElement.Offset;
            if (xboxElement.Stream == 0)
            {
                elementOffset = mainStreamSize;
                mainStreamSize += XboxVertexElement::GetTypeSize(mappedType);
            }

            D3DVertexElement element;
            element.Stream = xboxElement.Stream;
            element.Offset = static_cast<std::int16_t>(elementOffset);
            element.Type = XboxVertexElement::MapTypeToD3D(mappedType);
            element.Method = xboxElement.Method;
            element.Usage = xboxElement.Usage;
            element.UsageIndex = xboxElement.UsageIndex;
            AttributeStreamsInfo.push_back(element);

            offset += 12;
        }

        context.Position += 4;
        VertexStride = context.ReadInt32();
        int vertexDataSize = context.ReadInt32();
        VertexCount = VertexStride > 0 ? vertexDataSize / VertexStride : 0;

        int streamDataOffset = context.ReadPointer();
        auto streamData = context.LoadBuffer(streamDataOffset, VertexCount * streamSizes[0], true);
        Stream.assign(streamData.begin(), streamData.end());

        NumExtraStreams = context.ReadInt32();
        int extraStreamData = context.ReadPointer();
        context.Position += 0x20;

        VertexStreamFlags = context.ReadInt32();
        VertexStreamFlags |= 1;
        VertexStreamFlags &= ~0x0400;

        StreamHashes = context.LoadSharedPointer<VertexStreamHashes>();
        if (StreamHashes)
            StreamHashes->NumStreams = NumExtraStreams;

        if (extraStreamData != 0 && VertexCount > 0)
        {
            int streamSize = VertexCount * (NumExtraStreams + 1) * 0x8;
            auto extraStreamDataSpan = context.LoadBuffer(extraStreamData, streamSize, false);
            ExtraStream.assign(extraStreamDataSpan.begin(), extraStreamDataSpan.end());

            std::vector<std::uint8_t> converted;
            converted.resize(static_cast<std::size_t>(VertexCount) *
                             static_cast<std::size_t>(NumExtraStreams + 1) * 0xC);
            std::size_t outOffset = 0;
            for (std::size_t i = 0; i + 7 < ExtraStream.size(); i += 8)
            {
                std::uint16_t hx = static_cast<std::uint16_t>((ExtraStream[i] << 8) | ExtraStream[i + 1]);
                std::uint16_t hy = static_cast<std::uint16_t>((ExtraStream[i + 2] << 8) | ExtraStream[i + 3]);
                std::uint16_t hz = static_cast<std::uint16_t>((ExtraStream[i + 4] << 8) | ExtraStream[i + 5]);

                float fx = HalfToFloat(hx);
                float fy = HalfToFloat(hy);
                float fz = HalfToFloat(hz);

                std::memcpy(converted.data() + outOffset + 0, &fx, sizeof(float));
                std::memcpy(converted.data() + outOffset + 4, &fy, sizeof(float));
                std::memcpy(converted.data() + outOffset + 8, &fz, sizeof(float));
                outOffset += 0xC;
            }

            ExtraStream.swap(converted);
        }

        std::vector<std::uint8_t> remapped;
        remapped.resize(static_cast<std::size_t>(VertexCount) *
                        static_cast<std::size_t>(mainStreamSize));

        for (std::size_t i = 0; i < xboxElements.size(); ++i)
        {
            auto const& xboxElement = xboxElements[i];
            auto const& winElement = AttributeStreamsInfo[i];
            if (xboxElement.Stream != 0)
                continue;

            int xboxStreamSize = streamSizes[xboxElement.Stream];
            int winStreamSize = mainStreamSize;
            int xboxElementSize = XboxVertexElement::GetTypeSize(xboxElement.Type);
            int winElementSize = D3DVertexElement::GetTypeSize(winElement.Type);

            for (int v = 0; v < VertexCount; ++v)
            {
                int xboxElementOffset = v * xboxStreamSize + xboxElement.Offset;
                int winElementOffset = v * winStreamSize + winElement.Offset;

                if (xboxElementOffset + xboxElementSize > static_cast<int>(Stream.size()) ||
                    winElementOffset + winElementSize > static_cast<int>(remapped.size()))
                    continue;

                std::uint8_t* dst = remapped.data() + winElementOffset;
                std::uint8_t* src = Stream.data() + xboxElementOffset;

                switch (winElement.Type)
                {
                case D3DDeclType::D3DColor:
                case D3DDeclType::UByte4N:
                case D3DDeclType::UByte4:
                    std::memcpy(dst, src, static_cast<std::size_t>(winElementSize));
                    break;
                case D3DDeclType::Float1:
                case D3DDeclType::Float2:
                case D3DDeclType::Float3:
                case D3DDeclType::Float4:
                {
                    for (int w = 0; w < winElementSize; w += 4)
                    {
                        std::uint32_t val = (src[w] << 24) | (src[w + 1] << 16) |
                                            (src[w + 2] << 8) | src[w + 3];
                        dst[w + 0] = static_cast<std::uint8_t>((val >> 0) & 0xFF);
                        dst[w + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFF);
                        dst[w + 2] = static_cast<std::uint8_t>((val >> 16) & 0xFF);
                        dst[w + 3] = static_cast<std::uint8_t>((val >> 24) & 0xFF);
                    }
                    break;
                }
                case D3DDeclType::Short2:
                case D3DDeclType::Short4:
                case D3DDeclType::Short2N:
                case D3DDeclType::UShort4N:
                case D3DDeclType::Float16x2:
                case D3DDeclType::Float16x4:
                {
                    if (xboxElement.Type == XboxDeclType::Dec3N || xboxElement.Type == XboxDeclType::Dec4N)
                    {
                        std::uint32_t packed = (src[0] << 24) | (src[1] << 16) |
                                               (src[2] << 8) | src[3];
                        float x = DenormalizeSigned10BitInt(static_cast<std::uint16_t>((packed >> 0) & 0x3FF));
                        float y = DenormalizeSigned10BitInt(static_cast<std::uint16_t>((packed >> 10) & 0x3FF));
                        float z = DenormalizeSigned10BitInt(static_cast<std::uint16_t>((packed >> 20) & 0x3FF));
                        float w = 1.0f;
                        if (xboxElement.Type == XboxDeclType::Dec4N)
                            w = DenormalizeUnsigned3BitInt(static_cast<std::uint8_t>((packed >> 30) & 0x3));

                        std::uint16_t hx = FloatToHalf(x);
                        std::uint16_t hy = FloatToHalf(y);
                        std::uint16_t hz = FloatToHalf(z);
                        std::uint16_t hw = FloatToHalf(w);

                        dst[0] = static_cast<std::uint8_t>(hx & 0xFF);
                        dst[1] = static_cast<std::uint8_t>((hx >> 8) & 0xFF);
                        dst[2] = static_cast<std::uint8_t>(hy & 0xFF);
                        dst[3] = static_cast<std::uint8_t>((hy >> 8) & 0xFF);
                        dst[4] = static_cast<std::uint8_t>(hz & 0xFF);
                        dst[5] = static_cast<std::uint8_t>((hz >> 8) & 0xFF);
                        dst[6] = static_cast<std::uint8_t>(hw & 0xFF);
                        dst[7] = static_cast<std::uint8_t>((hw >> 8) & 0xFF);
                        break;
                    }

                    for (int w = 0; w < winElementSize; w += 2)
                    {
                        dst[w] = src[w + 1];
                        dst[w + 1] = src[w];
                    }
                    break;
                }
                default:
                    break;
                }
            }
        }

        Stream.swap(remapped);
        VertexStride = mainStreamSize;
    }
    else
    {
        std::size_t offset = static_cast<std::size_t>(attributeData);

        while (IsPointerRangeValid(cpuSpan, static_cast<int>(offset), 2))
        {
            if (context.ReadInt16(offset) == static_cast<std::int16_t>(0xFF))
                break;

            if (!IsPointerRangeValid(cpuSpan, static_cast<int>(offset), 8))
            {
                std::cerr << "[Forest] Vertex attribute descriptor truncated near offset: " << offset << std::endl;
                break;
            }

            D3DVertexElement element;
            element.Stream = context.ReadInt16(offset + 0);
            element.Offset = context.ReadInt16(offset + 2);
            element.Type = static_cast<D3DDeclType>(context.ReadInt8(offset + 4));
            element.Method = static_cast<D3DDeclMethod>(context.ReadInt8(offset + 5));
            element.Usage = static_cast<D3DDeclUsage>(context.ReadInt8(offset + 6));
            element.UsageIndex = static_cast<std::uint8_t>(context.ReadInt8(offset + 7));

            if (element.Stream >= 0 && element.Stream < kMaxStreams)
            {
                int size = element.Offset + D3DVertexElement::GetTypeSize(element.Type);
                streamSizes[static_cast<std::size_t>(element.Stream)] =
                    std::max(streamSizes[static_cast<std::size_t>(element.Stream)], size);
            }

            AttributeStreamsInfo.push_back(element);
            offset += 8;
        }

        context.Position += 4;
        NumExtraStreams = context.ReadInt32();
        if (NumExtraStreams < 0)
        {
            std::cerr << "[Forest] Extra stream count negative: " << NumExtraStreams << std::endl;
            NumExtraStreams = 0;
        }
        else if (NumExtraStreams > 64)
        {
            std::cerr << "[Forest] Extra stream count too large: " << NumExtraStreams << std::endl;
            NumExtraStreams = 64;
        }

        VertexStride = context.ReadInt32();
        if (VertexStride <= 0)
        {
            std::cerr << "[Forest] Vertex stride invalid: " << VertexStride << std::endl;
            VertexCount = 0;
            return;
        }
        if (VertexStride > 0x400)
        {
            std::cerr << "[Forest] Vertex stride exceeds limit: " << VertexStride << std::endl;
            return;
        }

        VertexCount = ClampCount(context.ReadInt32(), "RenderVertexStream.VertexCount");
        if (VertexCount == 0)
        {
            std::cerr << "[Forest] Vertex stream reports zero vertices" << std::endl;
        }

        int streamPtr = context.ReadPointer();
        if (streamPtr < 0)
        {
            std::cerr << "[Forest] Vertex stream pointer is negative: " << streamPtr << std::endl;
        }

        std::size_t expectedStreamBytes =
            static_cast<std::size_t>(VertexCount) * static_cast<std::size_t>(VertexStride);
        auto streamData = context.LoadBuffer(streamPtr, static_cast<int>(expectedStreamBytes), true);
        if (streamData.empty())
            streamData = context.LoadBuffer(streamPtr, static_cast<int>(expectedStreamBytes), false);

        if (VertexCount > 0 && expectedStreamBytes > streamData.size())
        {
            std::cerr << "[Forest] Vertex buffer truncated (expected " << expectedStreamBytes << " bytes, got "
                      << streamData.size() << ")" << std::endl;
            if (VertexStride > 0)
                VertexCount = static_cast<int>(streamData.size() / VertexStride);
            expectedStreamBytes = static_cast<std::size_t>(VertexCount) * static_cast<std::size_t>(VertexStride);
        }

        Stream.assign(streamData.begin(), streamData.begin() + expectedStreamBytes);
        if (Stream.empty())
        {
            std::cerr << "[Forest] Vertex stream is empty after clamping." << std::endl;
        }

        context.Position += 8;
        int extraStreamPtr = context.ReadPointer();
        VertexStreamFlags = context.ReadInt32();
        StreamHashes = context.LoadSharedPointer<VertexStreamHashes>();
        if (StreamHashes)
            StreamHashes->NumStreams = NumExtraStreams;

        if (extraStreamPtr != 0 && VertexCount > 0 && NumExtraStreams >= 0)
        {
            std::size_t extraStreamBytes = static_cast<std::size_t>(VertexCount) *
                (static_cast<std::size_t>(NumExtraStreams) + 1) * 0x0C;
            if (extraStreamBytes > 0)
            {
                int requestSize = static_cast<int>(std::min<std::size_t>(
                    extraStreamBytes,
                    static_cast<std::size_t>(std::numeric_limits<int>::max())));
                auto extraStreamData = context.LoadBuffer(extraStreamPtr, requestSize, false);
                if (extraStreamData.size() < static_cast<std::size_t>(requestSize))
                {
                    std::cerr << "[Forest] Extra vertex stream truncated (expected " << requestSize
                              << " bytes, got " << extraStreamData.size() << ")" << std::endl;
                }
                ExtraStream.assign(extraStreamData.begin(), extraStreamData.end());
            }
        }
    }

    StreamBias = 0;
    auto readFloatAt = [&](std::size_t offset) -> float {
        if (offset + 4 > Stream.size())
            return 0.0f;
        float v = 0.0f;
        std::memcpy(&v, Stream.data() + offset, sizeof(float));
        return v;
    };

    int positionOffset = -1;
    D3DDeclType positionType = D3DDeclType::Float3;
    for (auto const& attr : AttributeStreamsInfo)
    {
        if (attr.Stream == 0 && attr.Usage == D3DDeclUsage::Position)
        {
            positionOffset = attr.Offset;
            positionType = attr.Type;
            break;
        }
    }

    if (positionOffset >= 0 && VertexStride > 0 && !Stream.empty())
    {
        int sampleCount = std::min(VertexCount, 16);
        int bad0 = 0;
        int good4 = 0;
        for (int i = 0; i < sampleCount; ++i)
        {
            std::size_t base = static_cast<std::size_t>(i) * static_cast<std::size_t>(VertexStride);
            std::size_t off0 = base + static_cast<std::size_t>(positionOffset);
            std::size_t off4 = off0 + 4;
            if (positionType == D3DDeclType::Float3 || positionType == D3DDeclType::Float4)
            {
                float x0 = readFloatAt(off0);
                float x4 = readFloatAt(off4);
                if (std::abs(x0) < 1.0e-10f)
                    bad0++;
                if (std::abs(x4) > 1.0e-3f && std::abs(x4) < 1.0e6f)
                    good4++;
            }
        }
        if (sampleCount > 0 && bad0 >= sampleCount / 2 && good4 >= sampleCount / 2)
            StreamBias = 4;
    }
}

int SuRenderVertexStream::GetSizeForSerialization() const
{
    return 0x38;
}

void SuRenderPrimitive::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    ObbTransform = context.ReadMatrix(context.Position);
    context.Position += 0x40;
    Extents = context.ReadFloat4();
    CullSphere = context.ReadFloat4();

    for (int i = 0; i < 8; ++i)
        TextureMatrixIndices[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(context.ReadInt8());
    for (int i = 0; i < 8; ++i)
    {
        TextureUvMaps[static_cast<std::size_t>(i)] = {
            static_cast<std::uint8_t>(context.ReadInt8()),
            static_cast<std::uint8_t>(context.ReadInt8()),
            static_cast<std::uint8_t>(context.ReadInt8())
        };
    }
    TextureUvMapHashVS = context.ReadInt32();
    TextureUvMapHashPS = context.ReadInt32();

    Material = context.LoadSharedPointer<SuRenderMaterial>();
    NumIndices = ClampCount(context.ReadInt32(), "RenderPrimitive.NumIndices");
    int indexPtr = context.ReadPointer();
    if (indexPtr < 0)
    {
        std::cerr << "[Forest] Primitive index pointer negative: " << indexPtr << std::endl;
    }

    std::size_t requestedIndexBytes = static_cast<std::size_t>(NumIndices) * 2;
    auto indexData = context.LoadBuffer(indexPtr, static_cast<int>(requestedIndexBytes), true);
    if (indexData.empty())
        indexData = context.LoadBuffer(indexPtr, static_cast<int>(requestedIndexBytes), false);

    std::size_t availableIndexBytes = indexData.size();
    if (requestedIndexBytes > availableIndexBytes)
    {
        std::cerr << "[Forest] Primitive index buffer truncated (expected " << requestedIndexBytes
                  << " bytes, got " << availableIndexBytes << ")" << std::endl;
        NumIndices = static_cast<int>(availableIndexBytes / 2);
        requestedIndexBytes = static_cast<std::size_t>(NumIndices) * 2;
    }

    IndexData.assign(indexData.begin(), indexData.begin() + requestedIndexBytes);
    if (context.Platform && context.Platform->IsBigEndian())
    {
        for (std::size_t i = 0; i + 1 < IndexData.size(); i += 2)
            std::swap(IndexData[i], IndexData[i + 1]);
    }

    context.Position += 0x4;
    VertexStream = context.LoadSharedPointer<SuRenderVertexStream>();
    Unknown_0x9c = context.ReadInt32();
    Unknown_0xa0 = context.ReadInt32();
    context.ReadInt32();
}

int SuRenderPrimitive::GetSizeForSerialization() const
{
    return 0xA8;
}

void SuRenderMesh::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    BoundingSphere = context.ReadFloat4();
    int primitiveCount = ClampCount(context.ReadInt32(), "RenderMesh.Primitives");
    Primitives = context.LoadPointerArray<SuRenderPrimitive>(primitiveCount);

    int numBones = ClampCount(context.ReadInt32(), "RenderMesh.BoneIndices");
    BoneMatrixIndices = context.LoadArrayPointer(numBones, &SlLib::Serialization::ResourceLoadContext::ReadInt32);
    BoneInverseMatrices.clear();
    if (numBones > 0)
    {
        int address = context.ReadPointer();
        std::size_t link = context.Position;
        context.Position = static_cast<std::size_t>(address);
        BoneInverseMatrices.reserve(static_cast<std::size_t>(numBones));
        for (int i = 0; i < numBones; ++i)
        {
            BoneInverseMatrices.push_back(context.ReadMatrix(context.Position));
            context.Position += 0x40;
        }
        context.Position = link;
    }
    else
    {
        context.ReadPointer();
    }

    Name = context.ReadStringPointer();
}

int SuRenderMesh::GetSizeForSerialization() const
{
    return 0x28;
}

void SuLodThreshold::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    ThresholdDistance = context.ReadFloat();
    Mesh = context.LoadSharedPointer<SuRenderMesh>();
    ChildBranch = context.ReadInt16();
    context.Position += 2;
}

int SuLodThreshold::GetSizeForSerialization() const
{
    return 0xC;
}

void SuLodBranch::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int count = ClampCount(context.ReadInt32(), "LodBranch.Thresholds");
    Thresholds = context.LoadArrayPointer<SuLodThreshold>(count);
    context.Position += 4;
    ObbTransform = context.ReadMatrix(context.Position);
    context.Position += 0x40;
    Extents = context.ReadFloat4();
}

int SuLodBranch::GetSizeForSerialization() const
{
    return 0x5C;
}

void SuBranch::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Parent = context.ReadInt16();
    Child = context.ReadInt16();
    Sibling = context.ReadInt16();
    Flags = context.ReadInt16();
    context.ReadInt32();
    Name = context.ReadStringPointer();
    BlindData = context.LoadSharedPointer<SuBlindData>();

    bool isLodBranch = (Flags & 16) != 0;
    bool hasMeshData = (Flags & 8) != 0;

    if (isLodBranch)
        Lod = context.LoadSharedReference<SuLodBranch>();
    else if (hasMeshData)
        Mesh = context.LoadSharedPointer<SuRenderMesh>();
}

int SuBranch::GetSizeForSerialization() const
{
    if (Lod)
        return 0x70;
    return (Flags & 8) != 0 ? 0x18 : 0x14;
}

void SuCollisionTriangle::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    A = context.ReadFloat4();
    B = context.ReadFloat4();
    C = context.ReadFloat4();
}

int SuCollisionTriangle::GetSizeForSerialization() const
{
    return 0x30;
}

void SuCollisionMesh::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    ObbTransform = context.ReadMatrix(context.Position);
    context.Position += 0x40;
    Extents = context.ReadFloat4();
    Sphere = context.ReadFloat4();
    Hash = context.ReadInt32();
    Type = context.ReadInt16();
    BranchIndex = context.ReadInt16();
}

int SuCollisionMesh::GetSizeForSerialization() const
{
    return 0x70;
}

void SuLightData::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Type = context.ReadInt32();
    InnerConeAngle = context.ReadFloat();
    OuterConeAngle = context.ReadFloat();
    Falloff = context.ReadInt32();
    ShadowColor = context.ReadFloat4();
    IsShadowLight = context.ReadBoolean(true);
    BranchIndex = context.ReadInt32();
    Texture = context.LoadSharedPointer<SuRenderTexture>();
    for (int i = 0; i < 5; ++i)
        AnimatedData[static_cast<std::size_t>(i)] = context.ReadInt32();
}

int SuLightData::GetSizeForSerialization() const
{
    return 0x40;
}

void SuCameraData::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Branch = context.ReadInt32();
    Type = context.ReadInt32();
    Fov = context.ReadFloat();
    AnimatedData[0] = context.ReadInt32();
    AnimatedData[1] = context.ReadInt32();
}

int SuCameraData::GetSizeForSerialization() const
{
    return 0x14;
}

void SuField::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    BranchIndex = context.ReadInt32();
    Type = context.ReadInt32();
    Magnitude = context.ReadFloat();
    Attenuation = context.ReadFloat();
    Direction = context.ReadFloat4();
}

int SuField::GetSizeForSerialization() const
{
    return 0x20;
}

void SuRamp::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Shift = context.ReadInt32();
    Values.clear();
    Values.reserve(50);
    for (int i = 0; i < 50; ++i)
        Values.push_back(context.ReadInt32());
}

int SuRamp::GetSizeForSerialization() const
{
    return 0x204;
}

void SuCurve::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    BranchId = context.ReadInt32();
    Degree = context.ReadInt32();
    int numControlPoints = ClampCount(context.ReadInt32(), "Curve.ControlPoints");
    int numKnots = ClampCount(context.ReadInt32(), "Curve.Knots");
    ControlPoints.clear();
    if (numControlPoints > 0)
        ControlPoints.reserve(static_cast<std::size_t>(numControlPoints));
    for (int i = 0; i < numControlPoints; ++i)
        ControlPoints.push_back(context.ReadFloat4());
    Knots.clear();
    if (numKnots > 0)
        Knots.reserve(static_cast<std::size_t>(numKnots));
    for (int i = 0; i < numKnots; ++i)
        Knots.push_back(context.ReadFloat());
}

int SuCurve::GetSizeForSerialization() const
{
    return static_cast<int>(0x10 + ControlPoints.size() * 0x10 + Knots.size() * 0x4);
}

void SuEmitterData::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    BranchIndex = context.ReadInt32();
    Type = context.ReadInt32();
    SpeedMin = context.ReadFloat();
    SpeedMax = context.ReadFloat();
    LifeMin = context.ReadFloat();
    LifeMax = context.ReadFloat();
    MaxCount = context.ReadInt32();
    Spread = context.ReadFloat();
    Direction = context.ReadFloat4();
    RandomDirection = context.ReadFloat();
    DirectionalSpeed = context.ReadFloat();
    FaceVelocity = context.ReadInt32();
    InheritFactor = context.ReadFloat();
    ConserveFactor = context.ReadFloat();
    ConvertorFactor = context.ReadFloat();
    context.Position += 8;
    ParticleBranch = context.ReadInt32();
    Curviness = context.ReadFloat();
    UseFog = context.ReadInt32();
    Refraction = context.ReadInt32();
    VolumeShape = context.ReadInt32();
    VolumeSweep = context.ReadFloat();
    AwayFromCenter = context.ReadFloat();
    AlongAxis = context.ReadFloat();
    AroundAxis = context.ReadFloat();
    AwayFromAxis = context.ReadFloat();
    TangentSpeed = context.ReadFloat();
    NormalSpeed = context.ReadFloat();
    Rotation = context.ReadFloat();
    RotateSpeed = context.ReadFloat();
    Color = context.ReadInt32();
    RandomColor = context.ReadInt32();
    Width = context.ReadFloat();
    Height = context.ReadFloat();
    RandomScale = context.ReadFloat();
    ScrollSpeedU = context.ReadFloat();
    ScrollSpeedV = context.ReadFloat();
    Layer = context.ReadInt32();
    PlaneMode = context.ReadInt32();
    PlaneConst = context.ReadFloat();
    PlaneNormal = context.ReadFloat4();
    Flags = context.ReadInt32();

    int fieldCount = context.ReadInt32();
    Fields = context.LoadArrayPointer<SuField>(fieldCount);
    ColorRamp = context.LoadSharedPointer<SuRamp>();
    WidthRamp = context.LoadSharedPointer<SuRamp>();
    HeightRamp = context.LoadSharedPointer<SuRamp>();
    Curve = context.LoadSharedPointer<SuCurve>();
    Material = context.LoadSharedPointer<SuRenderMaterial>();
    Texture = context.LoadSharedPointer<SuRenderTexture>();

    AnimatedData[0] = context.ReadInt32();
    AnimatedData[1] = context.ReadInt32();
}

int SuEmitterData::GetSizeForSerialization() const
{
    return 0xF0;
}
void SuTextureTransform::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    PosU = context.ReadFloat();
    PosV = context.ReadFloat();
    Angle = context.ReadFloat();
    ScaleU = context.ReadFloat();
    ScaleV = context.ReadFloat();
}

int SuTextureTransform::GetSizeForSerialization() const
{
    return 0x14;
}

void StreamOverride::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Hash = context.ReadInt32();
    Index = context.ReadInt32();
}

int StreamOverride::GetSizeForSerialization() const
{
    return 0x8;
}

void SuAnimation::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int start = static_cast<int>(context.Position);
    Type = context.ReadInt32();
    NumFrames = context.ReadInt32();
    NumBones = context.ReadInt32();
    NumUvBones = context.ReadInt32();
    NumFloatStreams = context.ReadInt32();

    auto sane = [](int value, int maxValue) {
        return value >= 0 && value <= maxValue;
    };

    if (!sane(NumFrames, 200000) || !sane(NumBones, 8192) ||
        !sane(NumUvBones, 8192) || !sane(NumFloatStreams, 8192))
    {
        std::cerr << "[Forest] Animation header out of range: type=" << Type
                  << " frames=" << NumFrames << " bones=" << NumBones
                  << " uvBones=" << NumUvBones << " floatStreams=" << NumFloatStreams << std::endl;
        return;
    }

    if (Type == 0)
        return;

    if (Type == 6)
    {
        int bits = NumFloatStreams + 0x1F + (NumUvBones + NumBones) * 4;
        int numDoubleWords = ClampCount((bits + ((bits >> 0x1F) & 0x1F)) >> 5, "Animation6.Masks");
        if (numDoubleWords == 0)
            return;
        int wordDataOffset = 0x18 + (numDoubleWords * 4);
        int paramData = context.ReadInt32();
        if (!sane(paramData, static_cast<int>(context.Data().size())))
        {
            std::cerr << "[Forest] Animation type 6 invalid data pointer: " << paramData << std::endl;
            return;
        }

        ChannelMasks.clear();
        ChannelMasks.reserve(static_cast<std::size_t>(numDoubleWords));
        for (int i = 0; i < numDoubleWords; ++i)
            ChannelMasks.push_back(context.ReadInt32());

        int dataSize = 0;
        int maskIndex = 0;
        int headerStart = start + wordDataOffset;
        int headerOffset = headerStart;
        std::uint32_t mask = maskIndex < static_cast<int>(ChannelMasks.size())
            ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
            : 0u;
        int remaining = 8;

        for (int i = 0; i < NumBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0x12 + 0x11) & ~3;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0x18 + 0x13) & ~3;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0x12 + 0x11) & ~3;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 8) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        for (int i = 0; i < NumUvBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0xC + 0xF) & ~3;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0x18 + 0x13) & ~3;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0xC + 0xF) & ~3;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        remaining <<= 4;
        for (int i = 0; i < NumFloatStreams; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize = (dataSize + count * 0x6 + 0xD) & ~3;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 1;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 32;
            }
        }

        int numFrameHeaders = ClampCount((headerOffset - headerStart) / 2, "Animation6.Headers");
        int numPackedShorts = ClampCount(dataSize / 2, "Animation6.PackedShorts");
        if (numFrameHeaders == 0 || numPackedShorts == 0)
            return;

        AnimStreams.clear();
        AnimStreams.reserve(static_cast<std::size_t>(numFrameHeaders));
        for (int i = 0; i < numFrameHeaders; ++i)
            AnimStreams.push_back(context.ReadInt16());

        FrameData.clear();
        FrameData.reserve(static_cast<std::size_t>(numPackedShorts));
        for (int i = 0; i < numPackedShorts; ++i)
            FrameData.push_back(context.ReadInt16(static_cast<std::size_t>(paramData + (i * 2))));
    }
    else if (Type == 1)
    {
        int paramData = context.ReadInt32();
        if (!sane(paramData, static_cast<int>(context.Data().size())))
        {
            std::cerr << "[Forest] Animation type 1 invalid data pointer: " << paramData << std::endl;
            return;
        }
        VectorOffsets.clear();
        VectorOffsets.reserve(NumBones * 2);
        for (int i = 0; i < NumBones * 2; ++i)
            VectorOffsets.push_back(context.ReadInt32());

        int bits = NumFloatStreams + 0x1F + (NumUvBones + NumBones) * 4;
        int numDoubleWords = ClampCount((bits + ((bits >> 0x1F) & 0x1F)) >> 5, "Animation1.Masks");
        if (numDoubleWords == 0)
            return;
        int wordDataOffset = 0x18 + (NumBones * 8) + (numDoubleWords * 4);

        ChannelMasks.clear();
        ChannelMasks.reserve(static_cast<std::size_t>(numDoubleWords));
        for (int i = 0; i < numDoubleWords; ++i)
            ChannelMasks.push_back(context.ReadInt32());

        int dataSize = 0;
        int maskIndex = 0;
        int headerStart = start + wordDataOffset;
        int headerOffset = headerStart;
        std::uint32_t mask = maskIndex < static_cast<int>(ChannelMasks.size())
            ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
            : 0u;
        int remaining = 8;

        for (int i = 0; i < NumBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x30;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x40;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x30;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 8) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        for (int i = 0; i < NumUvBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x20;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x10;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x20;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        remaining <<= 4;
        for (int i = 0; i < NumFloatStreams; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += count * 0x10;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 1;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 32;
            }
        }

        int numFrameHeaders = ClampCount((headerOffset - headerStart) / 2, "Animation1.Headers");
        int numPackedFloats = ClampCount(dataSize / 0x10, "Animation1.PackedFloats");
        if (numFrameHeaders == 0 || numPackedFloats == 0)
            return;

        AnimStreams.clear();
        AnimStreams.reserve(static_cast<std::size_t>(numFrameHeaders));
        for (int i = 0; i < numFrameHeaders; ++i)
            AnimStreams.push_back(context.ReadInt16());

        VectorFrameData.clear();
        VectorFrameData.reserve(static_cast<std::size_t>(numPackedFloats));
        for (int i = 0; i < numPackedFloats; ++i)
            VectorFrameData.push_back(context.ReadFloat4(static_cast<std::size_t>(paramData + i * 0x10)));
    }
    else if (Type == 4)
    {
        int bits = NumFloatStreams + 0x1F + (NumUvBones + NumBones) * 4;
        int numDoubleWords = ClampCount((bits + ((bits >> 0x1F) & 0x1F)) >> 5, "Animation4.Masks");
        if (numDoubleWords == 0)
            return;
        int wordDataOffset = 0x18 + (numDoubleWords * 4);
        int paramData = context.ReadInt32();
        if (!sane(paramData, static_cast<int>(context.Data().size())))
        {
            std::cerr << "[Forest] Animation type 4 invalid data pointer: " << paramData << std::endl;
            return;
        }

        ChannelMasks.clear();
        ChannelMasks.reserve(static_cast<std::size_t>(numDoubleWords));
        for (int i = 0; i < numDoubleWords; ++i)
            ChannelMasks.push_back(context.ReadInt32());

        int dataSize = 0;
        int maskIndex = 0;
        int headerStart = start + wordDataOffset;
        int headerOffset = headerStart;
        std::uint32_t mask = maskIndex < static_cast<int>(ChannelMasks.size())
            ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
            : 0u;
        int remaining = 8;

        for (int i = 0; i < NumBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0xC + count * 0x24;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0xC + count * 0x30;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0xC + count * 0x24;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 8) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        for (int i = 0; i < NumUvBones; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0x8 + count * 0x18;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 2) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0x8 + count * 0x0C;
                headerOffset += 4 + (count * 2);
            }
            if ((mask & 4) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0x8 + count * 0x18;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 4;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 8;
            }
        }

        remaining <<= 4;
        for (int i = 0; i < NumFloatStreams; ++i)
        {
            if ((mask & 1) != 0)
            {
                int count = context.ReadInt16(static_cast<std::size_t>(headerOffset + 2));
                dataSize += 0x8 + count * 0x0C;
                headerOffset += 4 + (count * 2);
            }

            mask >>= 1;
            remaining -= 1;
            if (remaining == 0)
            {
                mask = maskIndex < static_cast<int>(ChannelMasks.size())
                    ? static_cast<std::uint32_t>(ChannelMasks[maskIndex++])
                    : 0u;
                remaining = 32;
            }
        }

        int numFrameHeaders = ClampCount((headerOffset - headerStart) / 2, "Animation4.Headers");
        int numPackedFloats = ClampCount(dataSize / 4, "Animation4.PackedFloats");
        if (numFrameHeaders == 0 || numPackedFloats == 0)
            return;

        AnimStreams.clear();
        AnimStreams.reserve(static_cast<std::size_t>(numFrameHeaders));
        for (int i = 0; i < numFrameHeaders; ++i)
            AnimStreams.push_back(context.ReadInt16());

        FloatFrameData.clear();
        FloatFrameData.reserve(static_cast<std::size_t>(numPackedFloats));
        for (int i = 0; i < numPackedFloats; ++i)
            FloatFrameData.push_back(context.ReadFloat(static_cast<std::size_t>(paramData + i * 4)));
    }
    else
    {
        std::cerr << "[Forest] Unsupported animation type: " << Type << std::endl;
    }
}

int SuAnimation::GetSizeForSerialization() const
{
    int size = 20;
    if (Type == 1)
        size += 0x4 + static_cast<int>(AnimStreams.size()) * 2 +
            static_cast<int>(ChannelMasks.size()) * 4 + (NumBones * 8);
    else if (Type == 4 || Type == 6)
        size += 0x4 + static_cast<int>(AnimStreams.size()) * 2 +
            static_cast<int>(ChannelMasks.size()) * 4;
    return size;
}

void SuAnimationEntry::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Animation = context.LoadSharedPointer<SuAnimation>();
    AnimName = context.ReadStringPointer();
    Hash = context.ReadInt32();
}

int SuAnimationEntry::GetSizeForSerialization() const
{
    return 0xC;
}

void SuTreeGroup::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    Hash = context.ReadInt32();
    int count = ClampCount(context.ReadInt32(), "TreeGroup.Hashes");
    TreeHashes = context.LoadArrayPointer(count, &SlLib::Serialization::ResourceLoadContext::ReadInt32);
}

int SuTreeGroup::GetSizeForSerialization() const
{
    return 0xC;
}

namespace {
SuTextureTransform ReadTextureTransform(SlLib::Serialization::ResourceLoadContext& context)
{
    SuTextureTransform t;
    t.Load(context);
    return t;
}
} // namespace

void SuRenderTree::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    BlindData = context.LoadSharedPointer<SuBlindData>();
    Hash = context.ReadInt32();

    int numBranches = ClampCount(context.ReadInt32(), "RenderTree.Branches");
    if (numBranches == 0)
        return;
    Branches = context.LoadPointerArray<SuBranch>(numBranches);
    Translations = context.LoadArrayPointer(numBranches, &SlLib::Serialization::ResourceLoadContext::ReadFloat4);
    Rotations = context.LoadArrayPointer(numBranches, &SlLib::Serialization::ResourceLoadContext::ReadFloat4);
    Scales = context.LoadArrayPointer(numBranches, &SlLib::Serialization::ResourceLoadContext::ReadFloat4);

    int numTextureMatrices = ClampCount(context.ReadInt32(), "RenderTree.TextureMatrices");
    CollisionMeshes = context.LoadPointerArray<SuCollisionMesh>(context.ReadInt32());
    Lights = context.LoadPointerArray<SuLightData>(context.ReadInt32());
    Cameras = context.LoadPointerArray<SuCameraData>(context.ReadInt32());
    Emitters = context.LoadPointerArray<SuEmitterData>(context.ReadInt32());
    Curves = context.LoadPointerArray<SuCurve>(context.ReadInt32());

    DefaultTextureTransforms = context.LoadArrayPointer(numTextureMatrices, ReadTextureTransform);
    int animationEntryCount = ClampCount(context.ReadInt32(), "RenderTree.AnimationEntries");
    AnimationEntries.clear();
    if (animationEntryCount > 0)
        AnimationEntries.reserve(static_cast<std::size_t>(animationEntryCount));
    int animEntryAddress = context.ReadPointer();
    if (animationEntryCount > 0 && animEntryAddress != 0)
    {
        std::size_t dataSize = context.Data().size();
        std::size_t animEntryEnd = static_cast<std::size_t>(animEntryAddress) +
            static_cast<std::size_t>(animationEntryCount) * 0xC;
        if (animationEntryCount == 0 || animEntryAddress < 0 || animEntryEnd > dataSize)
        {
            std::cerr << "[Forest] Animation entry pointer out of range: count="
                      << animationEntryCount << " ptr=" << animEntryAddress << std::endl;
        }
        else
        {
            auto isHeaderSane = [&](std::size_t animPtr) {
                if (animPtr + 0x14 > dataSize)
                    return false;
                int type = context.ReadInt32(animPtr);
                if (type != 0 && type != 1 && type != 4 && type != 6)
                    return false;
                int frames = context.ReadInt32(animPtr + 4);
                int bones = context.ReadInt32(animPtr + 8);
                int uvBones = context.ReadInt32(animPtr + 12);
                int floatStreams = context.ReadInt32(animPtr + 16);
                return frames >= 0 && frames <= 200000 &&
                    bones >= 0 && bones <= 8192 &&
                    uvBones >= 0 && uvBones <= 8192 &&
                    floatStreams >= 0 && floatStreams <= 8192;
            };

            std::size_t link = context.Position;
            for (int i = 0; i < animationEntryCount; ++i)
            {
                std::size_t entryOffset = static_cast<std::size_t>(animEntryAddress) + (static_cast<std::size_t>(i) * 0xC);
                if (entryOffset + 0xC > dataSize)
                    break;

                int animPtrRaw = context.ReadPointer(entryOffset);
                if (animPtrRaw == 0)
                    continue;

                std::size_t animPtr = static_cast<std::size_t>(animPtrRaw);
                if (animPtr >= dataSize || !isHeaderSane(animPtr))
                    continue;

                SuAnimationEntry entry;
                entry.AnimName = context.ReadStringPointer(entryOffset + 4);
                entry.Hash = context.ReadInt32(entryOffset + 8);

                context.Position = animPtr;
                entry.Animation = context.LoadSharedReference<SuAnimation>();
                AnimationEntries.push_back(std::move(entry));
            }
            context.Position = link;
        }
    }

    int floatCount = ClampCount(context.ReadInt32(), "RenderTree.AnimationFloats");
    DefaultAnimationFloats = context.LoadArrayPointer(floatCount, &SlLib::Serialization::ResourceLoadContext::ReadFloat);

    int overrideCount = ClampCount(context.ReadInt32(), "RenderTree.StreamOverrides");
    StreamOverrides.clear();
    if (overrideCount > 0)
        StreamOverrides.reserve(static_cast<std::size_t>(overrideCount));
    int overrideAddress = context.ReadPointer();
    if (overrideAddress != 0)
    {
        std::size_t link = context.Position;
        context.Position = static_cast<std::size_t>(overrideAddress);
        for (int i = 0; i < overrideCount; ++i)
        {
            StreamOverride entry;
            entry.Load(context);
            StreamOverrides.push_back(std::move(entry));
        }
        context.Position = link;
    }
}

int SuRenderTree::GetSizeForSerialization() const
{
    return 0x64;
}

void SuRenderForest::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    int treeCount = ClampCount(context.ReadInt32(), "RenderForest.Trees");
    Trees = context.LoadPointerArray<SuRenderTree>(treeCount);
    int texResCount = ClampCount(context.ReadInt32(), "RenderForest.TextureResources");
    TextureResources = context.LoadPointerArray<SuRenderTextureResource>(texResCount);
    int groupCount = ClampCount(context.ReadInt32(), "RenderForest.Groups");
    Groups = context.LoadArrayPointer<SuTreeGroup>(groupCount);
    int texCount = ClampCount(context.ReadInt32(), "RenderForest.Textures");
    Textures = context.LoadPointerArray<SuRenderTexture>(texCount);
    BlindData = context.LoadSharedPointer<SuBlindData>();
}

int SuRenderForest::GetSizeForSerialization() const
{
    return 0x24;
}

void ForestLibrary::Load(SlLib::Serialization::ResourceLoadContext& context)
{
    static SlLib::Resources::Database::SlPlatform s_win32("win32", false, false, 0);
    static SlLib::Resources::Database::SlPlatform s_xbox360("x360", true, false, 0);

    auto readForestCount = [&](SlLib::Resources::Database::SlPlatform* platform) {
        context.Platform = platform;
        return context.ReadInt32(0);
    };

    if (!context.Platform)
        context.Platform = &s_win32;

    SlLib::Resources::Database::SlPlatform* originalPlatform = context.Platform;
    int numForests = readForestCount(originalPlatform);
    if (numForests <= 0 || numForests > kMaxForestCount)
    {
        SlLib::Resources::Database::SlPlatform* alternate =
            originalPlatform && originalPlatform->IsBigEndian() ? &s_win32 : &s_xbox360;
        int swappedCount = readForestCount(alternate);
        if (swappedCount > 0 && swappedCount <= kMaxForestCount)
        {
            numForests = swappedCount;
        }
        else
        {
            context.Platform = originalPlatform;
        }
    }

    context.Position = 0;
    numForests = ClampCount(context.ReadInt32(), "ForestLibrary.Forests");
    Forests.clear();
    if (numForests == 0)
        return;
    Forests.reserve(static_cast<std::size_t>(numForests));

    for (int i = 0; i < numForests; ++i)
    {
        ForestEntry entry;
        entry.Hash = context.ReadInt32();
        entry.Name = context.ReadStringPointer();
        int forestData = context.ReadPointer();
        int gpuStart = context.ReadPointer();

        if (forestData <= 0 || static_cast<std::size_t>(forestData) >= context.Data().size())
        {
            std::cerr << "[Forest] Forest data pointer out of range for entry " << i << std::endl;
            continue;
        }

        std::span<const std::uint8_t> cpuSpan = context.Data().subspan(static_cast<std::size_t>(forestData));
        std::span<const std::uint8_t> gpuSpan;
        if (!context.GpuData().empty())
        {
            if (gpuStart == 0)
            {
                gpuSpan = context.GpuData();
            }
            else if (gpuStart > 0 && static_cast<std::size_t>(gpuStart) < context.GpuData().size())
            {
                gpuSpan = context.GpuData().subspan(static_cast<std::size_t>(gpuStart));
            }
        }

        SlLib::Serialization::ResourceLoadContext subcontext(cpuSpan, gpuSpan);
        subcontext.Platform = context.Platform;
        subcontext.Version = context.Version;

        auto forest = std::make_shared<SuRenderForest>();
        forest->Name = entry.Name;
        bool headerOk = true;
        auto checkPtr = [&](int ptr, char const* label) {
            if (ptr != 0 && (ptr < 0 || static_cast<std::size_t>(ptr) >= cpuSpan.size()))
            {
                std::cerr << "[Forest] Forest entry " << i << " invalid " << label
                          << " pointer: " << ptr << std::endl;
                headerOk = false;
            }
        };
        int treeCount = ClampCount(subcontext.ReadInt32(0), "RenderForest.Trees");
        int treePtr = subcontext.ReadInt32(4);
        int texResCount = ClampCount(subcontext.ReadInt32(8), "RenderForest.TextureResources");
        int texResPtr = subcontext.ReadInt32(12);
        int groupCount = ClampCount(subcontext.ReadInt32(16), "RenderForest.Groups");
        int groupPtr = subcontext.ReadInt32(20);
        int texCount = ClampCount(subcontext.ReadInt32(24), "RenderForest.Textures");
        int texPtr = subcontext.ReadInt32(28);
        checkPtr(treePtr, "tree");
        checkPtr(texResPtr, "texture resource");
        checkPtr(groupPtr, "group");
        checkPtr(texPtr, "texture");

        if (!headerOk)
        {
            std::cerr << "[Forest] Skipping forest entry " << i << " (" << entry.Name << ") due to invalid header.\n";
            continue;
        }

        try
        {
            forest->Load(subcontext);
            entry.Forest = forest;
        }
        catch (std::exception const& ex)
        {
            std::cerr << "[Forest] Failed to load forest entry " << i << ": " << ex.what() << std::endl;
            continue;
        }

        Forests.push_back(std::move(entry));
    }
}

} // namespace SeEditor::Forest
