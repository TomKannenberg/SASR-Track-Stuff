
#include "ForestTypes.hpp"

#include "SlLib/Resources/Database/SlPlatform.hpp"
#include "SlLib/Utilities/DdsUtil.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <optional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>

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

struct Type6Reader
{
    std::span<const std::uint8_t> Data;
    bool BigEndian = false;
    std::size_t Offset = 0;

    bool Has(std::size_t count) const { return Offset + count <= Data.size(); }

    std::uint8_t ReadU8()
    {
        if (!Has(1))
            return 0;
        return Data[Offset++];
    }

    std::uint16_t ReadU16()
    {
        if (!Has(2))
            return 0;
        std::uint16_t a = Data[Offset];
        std::uint16_t b = Data[Offset + 1];
        Offset += 2;
        if (BigEndian)
            return static_cast<std::uint16_t>((a << 8) | b);
        return static_cast<std::uint16_t>(a | (b << 8));
    }

    std::uint32_t ReadU32()
    {
        if (!Has(4))
            return 0;
        std::uint32_t b0 = Data[Offset + 0];
        std::uint32_t b1 = Data[Offset + 1];
        std::uint32_t b2 = Data[Offset + 2];
        std::uint32_t b3 = Data[Offset + 3];
        Offset += 4;
        if (BigEndian)
            return (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
        return b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    }

    float ReadFloat()
    {
        std::uint32_t bits = ReadU32();
        float v = 0.0f;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }
};

struct Type6StreamHeader
{
    std::uint32_t NumFrames = 0;
    std::uint32_t NumKeys = 0;
};

Type6StreamHeader ReadType6Header(Type6Reader& reader, bool smallHeader)
{
    Type6StreamHeader header{};
    if (smallHeader)
    {
        header.NumFrames = reader.ReadU8();
        header.NumKeys = reader.ReadU8();
    }
    else
    {
        header.NumFrames = reader.ReadU16();
        header.NumKeys = reader.ReadU16();
    }
    return header;
}

Type6StreamHeader ReadType6HeaderSafe(Type6Reader& reader,
                                      bool smallHeader,
                                      std::size_t maxFrames,
                                      std::size_t frameIndexSize,
                                      std::size_t factorSize)
{
    if (!smallHeader)
        return ReadType6Header(reader, false);

    std::size_t saved = reader.Offset;
    Type6StreamHeader h8 = ReadType6Header(reader, true);
    std::size_t minBytes = h8.NumFrames * (frameIndexSize + factorSize) + 8;
    std::size_t packedBytes = (static_cast<std::size_t>(h8.NumKeys) * 16 + 7) / 8;
    bool sane = h8.NumFrames <= maxFrames && reader.Has(minBytes + packedBytes);
    if (sane)
        return h8;

    reader.Offset = saved;
    return ReadType6Header(reader, false);
}

Type6StreamHeader ReadType6HeaderInlineSafe(Type6Reader& reader,
                                            bool smallHeader,
                                            std::size_t maxFrames,
                                            std::size_t frameIndexSize,
                                            std::size_t factorSize)
{
    if (!smallHeader)
        return ReadType6Header(reader, false);

    std::size_t saved = reader.Offset;
    Type6StreamHeader h8 = ReadType6Header(reader, true);
    std::size_t minBytes = (static_cast<std::size_t>(h8.NumKeys) + 1u) * frameIndexSize;
    bool sane = h8.NumFrames <= maxFrames && reader.Has(minBytes);
    if (sane)
        return h8;

    reader.Offset = saved;
    return ReadType6Header(reader, false);
}

std::uint32_t ReadBitsLSB(std::span<const std::uint8_t> data, std::size_t bitOffset, std::size_t bitCount)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bitCount; ++i)
    {
        std::size_t bitIndex = bitOffset + i;
        std::size_t byteIndex = bitIndex / 8;
        std::size_t bitInByte = bitIndex % 8;
        if (byteIndex >= data.size())
            break;
        if (data[byteIndex] & (1u << bitInByte))
            value |= (1u << i);
    }
    return value;
}

std::uint32_t ReadBitsMSB(std::span<const std::uint8_t> data, std::size_t bitOffset, std::size_t bitCount)
{
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < bitCount; ++i)
    {
        std::size_t bitIndex = bitOffset + i;
        std::size_t byteIndex = bitIndex / 8;
        std::size_t bitInByte = 7 - (bitIndex % 8);
        if (byteIndex >= data.size())
            break;
        if (data[byteIndex] & (1u << bitInByte))
            value |= (1u << (bitCount - 1 - i));
    }
    return value;
}

std::size_t Align4(std::size_t offset)
{
    return (offset + 3u) & ~static_cast<std::size_t>(3u);
}

std::size_t Align2(std::size_t offset)
{
    return (offset + 1u) & ~static_cast<std::size_t>(1u);
}

std::size_t ComputeType6MaskWordCount(int numBones, int numUvBones, int numFloatStreams)
{
    std::size_t bits = static_cast<std::size_t>(numBones + numUvBones) * 4u +
                       static_cast<std::size_t>(numFloatStreams);
    return (bits + 31u) >> 5;
}

std::uint32_t ReadMaskValue(std::span<const std::uint8_t> data,
                            std::size_t offset,
                            int entrySize,
                            bool bigEndian)
{
    if (entrySize == 1)
        return static_cast<std::uint8_t>(data[offset]);
    if (entrySize == 2)
    {
        std::uint16_t a = data[offset + 0];
        std::uint16_t b = data[offset + 1];
        return bigEndian ? static_cast<std::uint16_t>((a << 8) | b)
                         : static_cast<std::uint16_t>(a | (b << 8));
    }
    std::uint32_t b0 = data[offset + 0];
    std::uint32_t b1 = data[offset + 1];
    std::uint32_t b2 = data[offset + 2];
    std::uint32_t b3 = data[offset + 3];
    return bigEndian ? ((b0 << 24) | (b1 << 16) | (b2 << 8) | b3)
                     : (b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
}

bool ReadPackedNibbleMasks(std::span<const std::uint8_t> data,
                           std::size_t startOffset,
                           int numBones,
                           std::size_t maskWordCount,
                           bool bigEndian,
                           std::vector<std::uint32_t>& outMasks,
                           std::size_t& outMaskBytes)
{
    outMasks.clear();
    outMasks.reserve(static_cast<std::size_t>(numBones));
    std::size_t maskBytes = maskWordCount * 4u;
    if (startOffset + maskBytes > data.size())
        return false;
    for (int i = 0; i < numBones; ++i)
    {
        std::size_t dwordIndex = static_cast<std::size_t>(i) / 8u;
        std::size_t nibbleIndex = static_cast<std::size_t>(i) % 8u;
        std::size_t off = startOffset + dwordIndex * 4u;
        std::uint32_t dword = ReadMaskValue(data, off, 4, bigEndian);
        outMasks.push_back((dword >> (nibbleIndex * 4u)) & 0xFu);
    }
    outMaskBytes = maskBytes;
    return true;
}

bool TryParseType6Block(std::span<const std::uint8_t> data,
                        std::size_t startOffset,
                        std::size_t endOffset,
                        int type,
                        int numBones,
                        int numFrames,
                        int numUvBones,
                        int numFloatStreams,
                        bool bigEndian)
{
    if (startOffset >= endOffset || endOffset > data.size())
        return false;

    bool smallHeader = (type == 0x07 || type == 0x0A);
    std::size_t frameIndexSize = (type == 0x07 || type == 0x0A) ? 1u : 2u;
    std::size_t factorSize = 0;
    if (type == 0x08)
        factorSize = 4;
    else if (type == 0x09 || type == 0x0A)
        factorSize = 1;
    else
        factorSize = 2;

    Type6Reader reader{data, bigEndian, startOffset};
    std::vector<std::uint32_t> masks;
    std::size_t maskBytes = 0;
    std::size_t maskWords = ComputeType6MaskWordCount(numBones, numUvBones, numFloatStreams);
    if (!ReadPackedNibbleMasks(data, startOffset, numBones, maskWords, bigEndian, masks, maskBytes))
        return false;
    reader.Offset = startOffset + maskBytes;
    bool anySet = false;
    for (std::uint32_t mask : masks)
    {
        if (mask != 0)
        {
            anySet = true;
            break;
        }
    }
    if (!anySet)
        return false;
    int nonZeroCount = 0;
    for (std::uint32_t mask : masks)
    {
        if (mask != 0)
            ++nonZeroCount;
    }
    if (nonZeroCount < (numBones / 2))
        return false;

    auto advanceStream = [&](int streamCount) -> bool {
        for (int s = 0; s < streamCount; ++s)
        {
            Type6StreamHeader header = ReadType6Header(reader, smallHeader);
            std::size_t frames = header.NumFrames;
            std::size_t keys = header.NumKeys;
            if (frames == 0)
                frames = static_cast<std::size_t>(numFrames);
            if (frames == 0 || frames > static_cast<std::size_t>(numFrames))
                return false;
            if (keys == 0 || keys > 4096u)
                return false;
            std::size_t bytes = (keys + 1u) * frameIndexSize;
            std::size_t endOffset = Align2(reader.Offset + bytes);
            if (!reader.Has(endOffset - reader.Offset))
                return false;
            reader.Offset = endOffset;
        }
        return true;
    };

    for (int bone = 0; bone < numBones; ++bone)
    {
        std::uint32_t maskBits = masks[static_cast<std::size_t>(bone)];
        if (maskBits & 0x1u)
            if (!advanceStream(3)) return false;
        if (maskBits & 0x2u)
            if (!advanceStream(4)) return false;
        if (maskBits & 0x4u)
            if (!advanceStream(3)) return false;
        if (maskBits & 0x8u)
            if (!advanceStream(1)) return false;
    }

    return reader.Offset == endOffset;
}

bool FindType6BlockStart(std::span<const std::uint8_t> data,
                         std::size_t endOffset,
                         int type,
                         int numBones,
                         int numFrames,
                         int numUvBones,
                         int numFloatStreams,
                         bool bigEndian,
                         std::size_t& outStart)
{
    if (endOffset > data.size())
        return false;

    std::size_t maskWords = ComputeType6MaskWordCount(numBones, numUvBones, numFloatStreams);
    std::size_t maskBytes = maskWords * 4u;
    if (endOffset < maskBytes)
        return false;

    std::size_t maxScan = 8 * 1024 * 1024;
    std::size_t scanStart = (endOffset > maxScan) ? endOffset - maxScan : 0u;
    std::size_t firstCandidate = (endOffset >= maskBytes) ? (endOffset - maskBytes) : 0u;
    if (firstCandidate < scanStart)
        firstCandidate = scanStart;

    for (std::size_t candidate = firstCandidate; candidate + maskBytes <= endOffset; candidate += 4)
    {
        std::vector<std::uint32_t> masks;
        std::size_t dummyBytes = 0;
        if (!ReadPackedNibbleMasks(data, candidate, numBones, maskWords, bigEndian, masks, dummyBytes))
            continue;
        int nonZeroCount = 0;
        for (std::uint32_t mask : masks)
        {
            if (mask != 0)
                ++nonZeroCount;
        }
        if (nonZeroCount < (numBones / 2))
            continue;

        if (TryParseType6Block(data, candidate, endOffset, type, numBones, numFrames,
                               numUvBones, numFloatStreams, bigEndian))
        {
            outStart = candidate;
            return true;
        }
    }

    return false;
}

bool TryComputeType6End(std::span<const std::uint8_t> data,
                        std::size_t startOffset,
                        int type,
                        int numBones,
                        int numFrames,
                        int numUvBones,
                        int numFloatStreams,
                        bool bigEndian,
                        std::size_t& outEnd)
{
    if (startOffset >= data.size())
        return false;

    bool smallHeader = (type == 0x07 || type == 0x0A);
    std::size_t frameIndexSize = (type == 0x07 || type == 0x0A) ? 1u : 2u;
    std::size_t factorSize = 0;
    if (type == 0x08)
        factorSize = 4;
    else if (type == 0x09 || type == 0x0A)
        factorSize = 1;
    else
        factorSize = 2;

    Type6Reader reader{data, bigEndian, startOffset};
    std::vector<std::uint32_t> masks;
    std::size_t maskBytes = 0;
    std::size_t maskWords = ComputeType6MaskWordCount(numBones, numUvBones, numFloatStreams);
    if (!ReadPackedNibbleMasks(data, startOffset, numBones, maskWords, bigEndian, masks, maskBytes))
        return false;
    reader.Offset = startOffset + maskBytes;
    bool anySet = false;
    for (std::uint32_t mask : masks)
    {
        if (mask != 0)
        {
            anySet = true;
            break;
        }
    }
    if (!anySet)
        return false;
    int nonZeroCount = 0;
    for (std::uint32_t mask : masks)
    {
        if (mask != 0)
            ++nonZeroCount;
    }
    if (nonZeroCount < (numBones / 2))
        return false;

    auto advanceStream = [&](int streamCount) -> bool {
        for (int s = 0; s < streamCount; ++s)
        {
            Type6StreamHeader header = ReadType6Header(reader, smallHeader);
            std::size_t frames = header.NumFrames;
            std::size_t keys = header.NumKeys;
            if (frames == 0)
                frames = static_cast<std::size_t>(numFrames);
            if (frames == 0 || frames > static_cast<std::size_t>(numFrames))
                return false;
            if (keys == 0 || keys > 4096u)
                return false;
            std::size_t bytes = (keys + 1u) * frameIndexSize;
            std::size_t endOffset = Align2(reader.Offset + bytes);
            if (!reader.Has(endOffset - reader.Offset))
                return false;
            reader.Offset = endOffset;
        }
        return true;
    };

    for (int bone = 0; bone < numBones; ++bone)
    {
        std::uint32_t maskBits = masks[static_cast<std::size_t>(bone)];
        if (maskBits & 0x1u)
            if (!advanceStream(3)) return false;
        if (maskBits & 0x2u)
            if (!advanceStream(4)) return false;
        if (maskBits & 0x4u)
            if (!advanceStream(3)) return false;
        if (maskBits & 0x8u)
            if (!advanceStream(1)) return false;
    }

    if (reader.Offset <= startOffset || reader.Offset > data.size())
        return false;

    outEnd = reader.Offset;
    return true;
}

bool FindType6BlockAroundAnchor(std::span<const std::uint8_t> data,
                                std::size_t anchor,
                                int type,
                                int numBones,
                                int numFrames,
                                int numUvBones,
                                int numFloatStreams,
                                bool bigEndian,
                                std::size_t& outStart,
                                std::size_t& outEnd)
{
    if (data.empty() || anchor > data.size())
        return false;
    std::size_t maskWords = ComputeType6MaskWordCount(numBones, numUvBones, numFloatStreams);
    std::size_t maskBytes = maskWords * 4u;
    if (maskBytes == 0 || maskBytes > data.size())
        return false;

    std::size_t window = 256u * 1024u;
    std::size_t start = (anchor > window) ? (anchor - window) : 0u;
    std::size_t end = std::min(anchor + window, data.size());
    if (end < start + maskBytes)
        return false;

    bool smallHeader = (type == 0x07 || type == 0x0A);
    auto tryCandidate = [&](std::size_t candidate) -> bool {
        std::vector<std::uint32_t> masks;
        std::size_t dummyBytes = 0;
        if (!ReadPackedNibbleMasks(data, candidate, numBones, maskWords, bigEndian, masks, dummyBytes))
            return false;
        int nonZero = 0;
        for (std::uint32_t mask : masks)
        {
            if (mask != 0)
                ++nonZero;
        }
        if (nonZero < (numBones / 2))
            return false;

        std::size_t headerOff = candidate + maskBytes;
        if (headerOff + (smallHeader ? 2u : 4u) > data.size())
            return false;
        std::uint32_t hFrames = 0;
        std::uint32_t hKeys = 0;
        if (smallHeader)
        {
            hFrames = data[headerOff + 0];
            hKeys = data[headerOff + 1];
        }
        else
        {
            hFrames = data[headerOff + 0] | (static_cast<std::uint32_t>(data[headerOff + 1]) << 8);
            hKeys = data[headerOff + 2] | (static_cast<std::uint32_t>(data[headerOff + 3]) << 8);
        }
        if (hFrames == 0 || hFrames > static_cast<std::uint32_t>(numFrames))
            return false;
        if (hKeys == 0 || hKeys > 4096u)
            return false;

        std::size_t computedEnd = 0;
        if (TryComputeType6End(data, candidate, type, numBones, numFrames,
                               numUvBones, numFloatStreams, bigEndian, computedEnd))
        {
            outStart = candidate;
            outEnd = computedEnd;
            return true;
        }
        if (hKeys <= 1024u)
        {
            outStart = candidate;
            outEnd = data.size();
            return true;
        }
        return false;
    };

    std::size_t step = 4u;
    for (std::size_t candidate = anchor; candidate >= start; candidate -= step)
    {
        if (candidate + maskBytes <= end && tryCandidate(candidate))
            return true;
        if (candidate == 0)
            break;
    }
    for (std::size_t candidate = anchor + step;
         candidate + maskBytes <= end;
         candidate += step)
    {
        if (tryCandidate(candidate))
            return true;
    }
    return false;
}

bool FindType6StreamStartAroundAnchor(std::span<const std::uint8_t> data,
                                      std::size_t anchor,
                                      int type,
                                      int numFrames,
                                      bool bigEndian,
                                      std::size_t& outStart)
{
    if (data.empty() || anchor > data.size())
        return false;
    bool smallHeader = (type == 0x07 || type == 0x0A);
    std::size_t step = smallHeader ? 2u : 4u;
    std::size_t window = 4u * 1024u;
    std::size_t start = (anchor > window) ? (anchor - window) : 0u;
    std::size_t end = std::min(anchor + window, data.size());

    auto readHeader = [&](std::size_t offset, std::uint32_t& frames, std::uint32_t& keys) -> bool {
        if (offset + step > data.size())
            return false;
        if (smallHeader)
        {
            frames = data[offset + 0];
            keys = data[offset + 1];
        }
        else
        {
            if (bigEndian)
            {
                frames = (static_cast<std::uint32_t>(data[offset + 0]) << 8) | data[offset + 1];
                keys = (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
            }
            else
            {
                frames = data[offset + 0] | (static_cast<std::uint32_t>(data[offset + 1]) << 8);
                keys = data[offset + 2] | (static_cast<std::uint32_t>(data[offset + 3]) << 8);
            }
        }
        return true;
    };

    for (std::size_t offset = anchor; offset >= start; offset -= step)
    {
        std::uint32_t frames = 0;
        std::uint32_t keys = 0;
        if (readHeader(offset, frames, keys) && frames > 0 && frames <= static_cast<std::uint32_t>(numFrames) &&
            keys > 0 && keys <= 4096u)
        {
            outStart = offset;
            return true;
        }
        if (offset == 0)
            break;
    }
    for (std::size_t offset = anchor + step; offset + step <= end; offset += step)
    {
        std::uint32_t frames = 0;
        std::uint32_t keys = 0;
        if (readHeader(offset, frames, keys) && frames > 0 && frames <= static_cast<std::uint32_t>(numFrames) &&
            keys > 0 && keys <= 4096u)
        {
            outStart = offset;
            return true;
        }
    }
    return false;
}

bool FindType6StreamStartWithMasks(std::span<const std::uint8_t> data,
                                   std::size_t anchor,
                                   int type,
                                   int numBones,
                                   int numFrames,
                                   std::vector<int> const& channelMasks,
                                   bool bigEndian,
                                   std::size_t& outStart)
{
    if (data.empty() || anchor > data.size())
        return false;
    bool smallHeader = (type == 0x07 || type == 0x0A);
    std::size_t frameIndexSize = (type == 0x07 || type == 0x0A) ? 1u : 2u;
    std::size_t factorSize = 0;
    if (type == 0x08)
        factorSize = 4;
    else if (type == 0x09 || type == 0x0A)
        factorSize = 1;
    else
        factorSize = 2;

    std::vector<std::uint32_t> masks;
    masks.reserve(static_cast<std::size_t>(numBones));
    for (int bone = 0; bone < numBones; ++bone)
    {
        std::size_t wordIndex = static_cast<std::size_t>(bone) / 8u;
        std::size_t nibbleIndex = static_cast<std::size_t>(bone) % 8u;
        std::uint32_t word = wordIndex < channelMasks.size()
            ? static_cast<std::uint32_t>(channelMasks[wordIndex])
            : 0u;
        masks.push_back((word >> (nibbleIndex * 4u)) & 0xFu);
    }

    std::size_t step = smallHeader ? 2u : 4u;
    std::size_t window = 64u * 1024u;
    std::size_t start = (anchor > window) ? (anchor - window) : 0u;
    std::size_t end = std::min(anchor + window, data.size());

    for (std::size_t candidate = anchor; candidate >= start; candidate -= step)
    {
        Type6Reader reader{data, bigEndian, candidate};
        bool ok = true;
        for (int bone = 0; bone < numBones && ok; ++bone)
        {
            std::uint32_t maskBits = masks[static_cast<std::size_t>(bone)];
            int streamCounts[4] = {3, 4, 3, 1};
            for (int channel = 0; channel < 4 && ok; ++channel)
            {
                if ((maskBits & (1u << channel)) == 0)
                    continue;
                for (int s = 0; s < streamCounts[channel] && ok; ++s)
                {
                    if (!reader.Has(smallHeader ? 2u : 4u))
                    {
                        ok = false;
                        break;
                    }
                    Type6StreamHeader header = ReadType6Header(reader, smallHeader);
                    if (header.NumFrames == 0 || header.NumFrames > static_cast<std::uint32_t>(numFrames))
                    {
                        ok = false;
                        break;
                    }
                    if (header.NumKeys == 0 || header.NumKeys > 4096u)
                    {
                        ok = false;
                        break;
                    }
                    std::size_t bytes = static_cast<std::size_t>(header.NumFrames) * (frameIndexSize + factorSize);
                    reader.Offset += bytes;
                    reader.Offset = Align4(reader.Offset);
                    reader.Offset += 8;
                    std::size_t packedBytes = (static_cast<std::size_t>(header.NumKeys) * 16 + 7) / 8;
                    reader.Offset += packedBytes;
                    reader.Offset = Align4(reader.Offset);
                    if (reader.Offset > data.size())
                        ok = false;
                }
            }
        }
        if (ok)
        {
            outStart = candidate;
            return true;
        }
        if (candidate == 0)
            break;
    }
    for (std::size_t candidate = anchor + step; candidate + step <= end; candidate += step)
    {
        Type6Reader reader{data, bigEndian, candidate};
        bool ok = true;
        for (int bone = 0; bone < numBones && ok; ++bone)
        {
            std::uint32_t maskBits = masks[static_cast<std::size_t>(bone)];
            int streamCounts[4] = {3, 4, 3, 1};
            for (int channel = 0; channel < 4 && ok; ++channel)
            {
                if ((maskBits & (1u << channel)) == 0)
                    continue;
                for (int s = 0; s < streamCounts[channel] && ok; ++s)
                {
                    if (!reader.Has(smallHeader ? 2u : 4u))
                    {
                        ok = false;
                        break;
                    }
                    Type6StreamHeader header = ReadType6Header(reader, smallHeader);
                    if (header.NumFrames == 0 || header.NumFrames > static_cast<std::uint32_t>(numFrames))
                    {
                        ok = false;
                        break;
                    }
                    if (header.NumKeys == 0 || header.NumKeys > 4096u)
                    {
                        ok = false;
                        break;
                    }
                    std::size_t bytes = static_cast<std::size_t>(header.NumFrames) * (frameIndexSize + factorSize);
                    reader.Offset += bytes;
                    reader.Offset = Align4(reader.Offset);
                    reader.Offset += 8;
                    std::size_t packedBytes = (static_cast<std::size_t>(header.NumKeys) * 16 + 7) / 8;
                    reader.Offset += packedBytes;
                    reader.Offset = Align4(reader.Offset);
                    if (reader.Offset > data.size())
                        ok = false;
                }
            }
        }
        if (ok)
        {
            outStart = candidate;
            return true;
        }
    }
    return false;
}

bool FindType6StreamStartByMaskBytes(std::span<const std::uint8_t> data,
                                     std::size_t anchor,
                                     int type,
                                     int numBones,
                                     int numFrames,
                                     bool bigEndian,
                                     std::size_t& outStart)
{
    if (data.empty() || anchor > data.size())
        return false;
    bool smallHeader = (type == 0x07 || type == 0x0A);
    std::size_t step = smallHeader ? 2u : 4u;
    std::size_t window = 4u * 1024u;
    std::size_t start = (anchor > window) ? (anchor - window) : 0u;
    std::size_t end = std::min(anchor + window, data.size());
    std::size_t maskBytes = static_cast<std::size_t>(numBones);
    if (maskBytes == 0 || maskBytes > data.size())
        return false;

    auto readHeader = [&](std::size_t offset, std::uint32_t& frames, std::uint32_t& keys) -> bool {
        if (offset + step > data.size())
            return false;
        if (smallHeader)
        {
            frames = data[offset + 0];
            keys = data[offset + 1];
        }
        else
        {
            if (bigEndian)
            {
                frames = (static_cast<std::uint32_t>(data[offset + 0]) << 8) | data[offset + 1];
                keys = (static_cast<std::uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
            }
            else
            {
                frames = data[offset + 0] | (static_cast<std::uint32_t>(data[offset + 1]) << 8);
                keys = data[offset + 2] | (static_cast<std::uint32_t>(data[offset + 3]) << 8);
            }
        }
        return true;
    };

    auto validMaskRun = [&](std::size_t candidate) -> bool {
        if (candidate + maskBytes > data.size())
            return false;
        int nonZero = 0;
        for (int i = 0; i < numBones; ++i)
        {
            std::uint8_t v = data[candidate + static_cast<std::size_t>(i)];
            if ((v & 0xFu) != 0)
                ++nonZero;
        }
        return nonZero >= (numBones / 2);
    };

    std::size_t best = 0;
    std::uint32_t bestFrames = 0;
    for (std::size_t candidate = start; candidate + maskBytes <= end; candidate += step)
    {
        if (!validMaskRun(candidate))
            continue;
        std::uint32_t frames = 0;
        std::uint32_t keys = 0;
        std::size_t headerOff = candidate + maskBytes;
        if (readHeader(headerOff, frames, keys) && frames > 0 && frames <= static_cast<std::uint32_t>(numFrames) &&
            keys > 0 && keys <= 4096u)
        {
            if (frames > bestFrames)
            {
                bestFrames = frames;
                best = headerOff;
            }
        }
    }
    if (bestFrames > 0)
    {
        outStart = best;
        return true;
    }
    return false;
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

void SuAnimationQuantization::Resize(std::size_t bones)
{
    Translation.assign(bones, {});
    Rotation.assign(bones, {});
    Scale.assign(bones, {});
    Visibility.assign(bones, {});
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

    if (Type >= 6 && Type <= 10)
    {
        bool paramGpu = false;
        int paramData = context.ReadPointer(paramGpu);
        if (!paramGpu && !sane(paramData, static_cast<int>(context.Data().size())))
        {
            std::cerr << "[Forest] Animation type 6 invalid data pointer: " << paramData << std::endl;
            return;
        }
        Type6BigEndian = context.Platform ? context.Platform->IsBigEndian() : false;
        Type6Block.clear();
        SamplesDecoded = false;
        Type6DataPtr = nullptr;
        Type6DataSize = 0;
        Type6GpuDataPtr = nullptr;
        Type6GpuDataSize = 0;
        Type6ParamDataIsGpu = paramGpu;
        Type6ParamDataOffset = paramData;
        Type6Anchor = 0;
        Type6BlockStart = 0;
        Type6BlockEnd = 0;
        Type6DebugWindow.clear();
        Type6DebugWindowOffset = 0;

        ChannelMasks.clear();
        AnimStreams.clear();
        FrameData.clear();

        auto dataSpan = context.Data();
        std::size_t anchor = static_cast<std::size_t>(paramData);
        Type6DataPtr = dataSpan.data();
        Type6DataSize = dataSpan.size();
        auto gpuSpan = context.GpuData();
        Type6GpuDataPtr = gpuSpan.data();
        Type6GpuDataSize = gpuSpan.size();
        std::size_t maskStart = static_cast<std::size_t>(start) + 0x18;
        Type6Anchor = maskStart;
        if (!dataSpan.empty())
        {
            std::size_t window = 256u * 1024u;
            std::size_t windowStart = (maskStart > window) ? (maskStart - window) : 0u;
            std::size_t windowEnd = std::min(maskStart + window, dataSpan.size());
            if (windowEnd > windowStart)
            {
                Type6DebugWindow.assign(dataSpan.begin() + static_cast<std::ptrdiff_t>(windowStart),
                                        dataSpan.begin() + static_cast<std::ptrdiff_t>(windowEnd));
                Type6DebugWindowOffset = windowStart;
            }
        }

        if (maskStart <= dataSpan.size())
        {
            std::size_t blockEnd = 0;
            auto tryComputeBlock = [&](bool bigEndian, std::size_t candidate, std::size_t& outEnd) -> bool {
                return TryComputeType6End(dataSpan, candidate, Type, NumBones, NumFrames,
                                          NumUvBones, NumFloatStreams, bigEndian, outEnd);
            };

            bool found = tryComputeBlock(Type6BigEndian, maskStart, blockEnd);
            if (!found)
            {
                bool otherEndian = !Type6BigEndian;
                found = tryComputeBlock(otherEndian, maskStart, blockEnd);
                if (found)
                    Type6BigEndian = otherEndian;
            }
            if (!found)
            {
                std::size_t blockStart = 0;
                if (FindType6BlockStart(dataSpan, maskStart, Type, NumBones, NumFrames,
                                        NumUvBones, NumFloatStreams, Type6BigEndian, blockStart))
                {
                    found = tryComputeBlock(Type6BigEndian, blockStart, blockEnd);
                    if (!found)
                    {
                        bool otherEndian = !Type6BigEndian;
                        found = tryComputeBlock(otherEndian, blockStart, blockEnd);
                        if (found)
                            Type6BigEndian = otherEndian;
                    }
                    if (found)
                        maskStart = blockStart;
                }
            }

            if (found)
            {
                Type6MaskEntrySize = 0;
                Type6BlockStart = maskStart;
                Type6BlockEnd = blockEnd;
                if (!paramGpu &&
                    paramData > static_cast<int>(Type6BlockStart) &&
                    paramData < static_cast<int>(Type6BlockEnd))
                {
                    Type6BlockEnd = static_cast<std::size_t>(paramData);
                }
                Type6Block.assign(dataSpan.begin() + static_cast<std::ptrdiff_t>(Type6BlockStart),
                                  dataSpan.begin() + static_cast<std::ptrdiff_t>(Type6BlockEnd));
            }
        }

        if (Type6Block.empty() && maskStart < dataSpan.size())
        {
            Type6MaskEntrySize = 0;
            Type6BlockStart = maskStart;
            Type6BlockEnd = dataSpan.size();
            Type6Block.assign(dataSpan.begin() + static_cast<std::ptrdiff_t>(maskStart), dataSpan.end());
        }
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
    else if (Type == 4 || (Type >= 6 && Type <= 10))
        size += 0x4 + static_cast<int>(AnimStreams.size()) * 2 +
            static_cast<int>(ChannelMasks.size()) * 4;
    return size;
}

SuAnimationSample const* SuAnimation::GetSample(int frame, int bone) const
{
    if (frame < 0 || bone < 0)
        return nullptr;
    if (NumFrames <= 0 || NumBones <= 0)
        return nullptr;
    if (static_cast<std::size_t>(frame) >= static_cast<std::size_t>(NumFrames))
        return nullptr;
    if (static_cast<std::size_t>(bone) >= static_cast<std::size_t>(NumBones))
        return nullptr;
    if (Samples.size() != static_cast<std::size_t>(NumFrames) * static_cast<std::size_t>(NumBones))
        return nullptr;
    return &Samples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                    static_cast<std::size_t>(bone)];
}

bool SuAnimation::DecodeType6Samples(SuRenderTree const& tree)
{
    if (Type < 0x06 || Type > 0x0A)
        return false;
    if (NumFrames <= 0 || NumBones <= 0)
        return false;
    if (Type6Block.empty())
        return false;
    std::span<const std::uint8_t> blockSpan{Type6Block.data(), Type6Block.size()};
    std::span<const std::uint8_t> fullSpan{};
    if (Type6ParamDataIsGpu)
    {
        if (Type6GpuDataPtr != nullptr && Type6GpuDataSize > 0)
            fullSpan = std::span<const std::uint8_t>(Type6GpuDataPtr, Type6GpuDataSize);
    }
    else
    {
        if (Type6DataPtr != nullptr && Type6DataSize > 0)
            fullSpan = std::span<const std::uint8_t>(Type6DataPtr, Type6DataSize);
    }
    bool debugDump = std::getenv("TYPE6_DUMP") != nullptr || std::getenv("DECODE_DEBUG") != nullptr;
    if (SamplesDecoded && !debugDump)
        return true;

    auto initSamples = [&](std::vector<SuAnimationSample>& samples) {
        samples.clear();
        samples.resize(static_cast<std::size_t>(NumFrames) * static_cast<std::size_t>(NumBones));
        for (int bone = 0; bone < NumBones; ++bone)
        {
            SlLib::Math::Vector4 t{};
            SlLib::Math::Vector4 r{};
            SlLib::Math::Vector4 s{1.0f, 1.0f, 1.0f, 1.0f};
            if (static_cast<std::size_t>(bone) < tree.Translations.size())
                t = tree.Translations[static_cast<std::size_t>(bone)];
            if (static_cast<std::size_t>(bone) < tree.Rotations.size())
                r = tree.Rotations[static_cast<std::size_t>(bone)];
            if (static_cast<std::size_t>(bone) < tree.Scales.size())
                s = tree.Scales[static_cast<std::size_t>(bone)];

            for (int frame = 0; frame < NumFrames; ++frame)
            {
                auto& sample = samples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                       static_cast<std::size_t>(bone)];
                sample.Translation = t;
                sample.Rotation = r;
                sample.Scale = s;
                sample.Visible = true;
            }
        }
    };

    auto decodeType6UsUs = [&](bool bigEndian,
                               std::span<const std::uint8_t> streamData,
                               std::size_t maskWords,
                               std::span<const std::uint8_t> paramData,
                               std::size_t paramOffset,
                               std::vector<SuAnimationSample>& outSamples,
                               SuAnimationQuantization& outQuant,
                               std::vector<bool>& outHasRotation,
                               int& outMaskNonZero,
                               std::array<std::uint32_t, 4>& outMaskSample,
                               int& outMaskScore) -> bool {
        std::vector<std::uint32_t> masks;
        std::size_t maskBytes = 0;
        if (streamData.empty())
            return false;
        if (!ReadPackedNibbleMasks(streamData, 0, NumBones, maskWords, bigEndian, masks, maskBytes))
            return false;

        initSamples(outSamples);
        outQuant.Resize(static_cast<std::size_t>(NumBones));
        outHasRotation.assign(static_cast<std::size_t>(NumBones), false);
        outMaskNonZero = 0;
        int transCount = 0;
        int rotCount = 0;
        int scaleCount = 0;
        int visCount = 0;
        for (std::uint32_t m : masks)
        {
            if (m != 0)
                ++outMaskNonZero;
            if (m & 0x1u) ++transCount;
            if (m & 0x2u) ++rotCount;
            if (m & 0x4u) ++scaleCount;
            if (m & 0x8u) ++visCount;
        }
        for (std::size_t i = 0; i < outMaskSample.size(); ++i)
            outMaskSample[i] = (i < masks.size()) ? masks[i] : 0u;
        outMaskScore = outMaskNonZero * 2 + transCount * 10 + scaleCount * 6 + visCount * 2 + rotCount;

        auto readU16 = [&](std::span<const std::uint8_t> data, std::size_t& offset) -> std::uint16_t {
            if (offset + 2 > data.size())
                return 0;
            std::uint16_t value = 0;
            if (bigEndian)
                value = static_cast<std::uint16_t>(data[offset] << 8 | data[offset + 1]);
            else
                value = static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
            offset += 2;
            return value;
        };
        auto readS16At = [&](std::span<const std::uint8_t> data, std::size_t offset, bool useBigEndian) -> std::int16_t {
            if (offset + 2 > data.size())
                return 0;
            std::uint16_t value = 0;
            if (useBigEndian)
                value = static_cast<std::uint16_t>(data[offset] << 8 | data[offset + 1]);
            else
                value = static_cast<std::uint16_t>(data[offset] | (data[offset + 1] << 8));
            return static_cast<std::int16_t>(value);
        };
        auto readF32At = [&](std::span<const std::uint8_t> data, std::size_t offset, bool useBigEndian) -> float {
            if (offset + 4 > data.size())
                return 0.0f;
            std::uint32_t raw = 0;
            if (useBigEndian)
                raw = (static_cast<std::uint32_t>(data[offset]) << 24) |
                      (static_cast<std::uint32_t>(data[offset + 1]) << 16) |
                      (static_cast<std::uint32_t>(data[offset + 2]) << 8) |
                      static_cast<std::uint32_t>(data[offset + 3]);
            else
                raw = static_cast<std::uint32_t>(data[offset]) |
                      (static_cast<std::uint32_t>(data[offset + 1]) << 8) |
                      (static_cast<std::uint32_t>(data[offset + 2]) << 16) |
                      (static_cast<std::uint32_t>(data[offset + 3]) << 24);
            float value = 0.0f;
            std::memcpy(&value, &raw, sizeof(value));
            return value;
        };

        std::size_t streamOffset = maskBytes;
        if (paramData.empty())
            paramData = fullSpan.empty() ? blockSpan : fullSpan;
        paramOffset = Align4(paramOffset);
        const float invQuant = 1.0f / 32768.0f;

        // Heuristic: param block endianness can differ from stream endianness.
        // Pick the endian that yields reasonable min/delta.
        bool paramBigEndian = bigEndian;
        if (paramOffset + 8 <= paramData.size())
        {
            float minBe = readF32At(paramData, paramOffset, true);
            float deltaBe = readF32At(paramData, paramOffset + 4, true);
            float minLe = readF32At(paramData, paramOffset, false);
            float deltaLe = readF32At(paramData, paramOffset + 4, false);
            auto ok = [](float v) { return std::isfinite(v) && std::fabs(v) < 1.0e4f; };
            bool beOk = ok(minBe) && ok(deltaBe) && deltaBe >= 0.0f;
            bool leOk = ok(minLe) && ok(deltaLe) && deltaLe >= 0.0f;
            if (!beOk && leOk)
                paramBigEndian = false;
            else if (!leOk && beOk)
                paramBigEndian = true;
            else if (leOk && beOk)
            {
                // Prefer the smaller magnitude pair.
                float magBe = std::fabs(minBe) + std::fabs(deltaBe);
                float magLe = std::fabs(minLe) + std::fabs(deltaLe);
                if (magLe < magBe)
                    paramBigEndian = false;
            }
        }

        auto findKeyIndex = [&](std::vector<std::uint16_t> const& keyTimes, int frame) -> int {
            int numKeys = static_cast<int>(keyTimes.size());
            if (numKeys <= 0)
                return 0;
            if (numKeys < 5)
            {
                int idx = 0;
                for (; idx < numKeys; ++idx)
                {
                    if (frame < static_cast<int>(keyTimes[static_cast<std::size_t>(idx)]))
                        break;
                }
                if (idx >= numKeys)
                    idx = numKeys - 1;
                return idx;
            }

            int lastTime = static_cast<int>(keyTimes[static_cast<std::size_t>(numKeys - 1)]);
            int idx = (lastTime > 0) ? (frame * numKeys) / lastTime : 0;
            if (idx < 0)
                idx = 0;
            if (idx >= numKeys)
                idx = numKeys - 1;
            if (static_cast<int>(keyTimes[static_cast<std::size_t>(idx)]) <= frame)
            {
                while (idx + 1 < numKeys &&
                       static_cast<int>(keyTimes[static_cast<std::size_t>(idx + 1)]) <= frame)
                {
                    ++idx;
                }
            }
            else
            {
                while (idx > 0 &&
                       static_cast<int>(keyTimes[static_cast<std::size_t>(idx - 1)]) > frame)
                {
                    --idx;
                }
            }
            return idx;
        };

        auto decodeVecStream = [&](int boneIndex, int componentCount, int channel) {
            std::size_t localStream = streamOffset;
            std::uint16_t numFrames = readU16(streamData, localStream);
            std::uint16_t numKeys = readU16(streamData, localStream);
            if (numFrames == 0 || numKeys == 0)
            {
                streamOffset = localStream;
                return;
            }
            std::vector<std::uint16_t> keyTimes;
            keyTimes.reserve(numKeys);
            for (std::size_t i = 0; i < numKeys; ++i)
                keyTimes.push_back(readU16(streamData, localStream));
            streamOffset = localStream;

            paramOffset = Align4(paramOffset);
            float minimum = readF32At(paramData, paramOffset, paramBigEndian);
            float delta = readF32At(paramData, paramOffset + 4, paramBigEndian);
            std::size_t baseSamples = paramOffset + 8;
            std::size_t keyStrideBytes = static_cast<std::size_t>(componentCount) * 3u * 2u;
            std::size_t tailOffset = baseSamples + static_cast<std::size_t>(numKeys) * keyStrideBytes;

            for (int frame = 0; frame < NumFrames; ++frame)
            {
                int localFrame = frame % static_cast<int>(numFrames);
                if (localFrame < 0)
                    localFrame = 0;
                int k = findKeyIndex(keyTimes, localFrame);
                if (k < 0)
                    k = 0;
                if (k >= static_cast<int>(numKeys))
                    k = static_cast<int>(numKeys) - 1;

                int prevTime = (k == 0) ? 0 : static_cast<int>(keyTimes[static_cast<std::size_t>(k - 1)]);
                int nextTime = static_cast<int>(keyTimes[static_cast<std::size_t>(k)]);
                int denom = nextTime - prevTime;
                float t = 0.0f;
                if (denom > 0)
                    t = static_cast<float>(localFrame - prevTime) / static_cast<float>(denom);
                float ts = t * t;
                float tc = ts * t;

                SlLib::Math::Vector4 value{};
                for (int c = 0; c < componentCount; ++c)
                {
                    auto readSampleAt = [&](int keyIndex, int sampleSet) -> float {
                        std::size_t off = baseSamples +
                                          static_cast<std::size_t>(keyIndex) * keyStrideBytes +
                                          static_cast<std::size_t>(sampleSet) *
                                              static_cast<std::size_t>(componentCount) * 2u +
                                          static_cast<std::size_t>(c) * 2u;
                        std::int16_t raw = readS16At(paramData, off, paramBigEndian);
                        float normalized = static_cast<float>(raw) * invQuant;
                        return minimum + delta * normalized;
                    };
                    auto readTail = [&]() -> float {
                        std::size_t off = tailOffset + static_cast<std::size_t>(c) * 2u;
                        std::int16_t raw = readS16At(paramData, off, paramBigEndian);
                        float normalized = static_cast<float>(raw) * invQuant;
                        return minimum + delta * normalized;
                    };

                    float p0 = readSampleAt(k, 0);
                    float p1 = readSampleAt(k, 1);
                    float p2 = readSampleAt(k, 2);
                    float p3 = (k + 1 < static_cast<int>(numKeys)) ? readSampleAt(k + 1, 0) : readTail();
                    float v = (((p3 - p2) - p1) - p0) * tc + p2 * ts + p1 * t + p0;
                    if (c == 0)
                        value.X = v;
                    else if (c == 1)
                        value.Y = v;
                    else if (c == 2)
                        value.Z = v;
                    else if (c == 3)
                        value.W = v;
                }

                auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                       static_cast<std::size_t>(boneIndex)];
                auto finiteVec = [&](SlLib::Math::Vector4 const& v) -> bool {
                    return std::isfinite(v.X) && std::isfinite(v.Y) && std::isfinite(v.Z) && std::isfinite(v.W);
                };
                if (!finiteVec(value))
                    return;
                if (channel == 0)
                    sample.Translation = value;
                else if (channel == 1)
                {
                    // Avoid zero-length quaternions that can poison skinning.
                    float len2 = value.X * value.X + value.Y * value.Y + value.Z * value.Z + value.W * value.W;
                    if (len2 > 1.0e-8f)
                        sample.Rotation = value;
                }
                else if (channel == 2)
                    sample.Scale = value;
            }

            std::size_t advance = Align4(((static_cast<std::size_t>(numKeys) *
                                           static_cast<std::size_t>(componentCount) * 3u +
                                           static_cast<std::size_t>(componentCount)) *
                                          2u + 8u));
            paramOffset += advance;
        };

        auto decodeVisibility = [&](int boneIndex) {
            std::size_t localStream = streamOffset;
            std::uint16_t numFrames = readU16(streamData, localStream);
            std::uint16_t numKeys = readU16(streamData, localStream);
            if (numFrames == 0 || numKeys == 0)
            {
                streamOffset = localStream;
                return;
            }
            std::vector<std::uint16_t> keyTimes;
            keyTimes.reserve(numKeys);
            for (std::size_t i = 0; i < numKeys; ++i)
                keyTimes.push_back(readU16(streamData, localStream));
            streamOffset = localStream;

            for (int frame = 0; frame < NumFrames; ++frame)
            {
                int localFrame = frame % static_cast<int>(numFrames);
                if (localFrame < 0)
                    localFrame = 0;
                int k = findKeyIndex(keyTimes, localFrame);
                bool visible = (k & 1) == 0;
                auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                       static_cast<std::size_t>(boneIndex)];
                sample.Visible = visible;
            }
        };

        for (int bone = 0; bone < NumBones; ++bone)
        {
            std::uint32_t mask = (static_cast<std::size_t>(bone) < masks.size())
                                     ? masks[static_cast<std::size_t>(bone)]
                                     : 0u;
            if ((mask & 1u) != 0)
                decodeVecStream(bone, 3, 0);
            if ((mask & 2u) != 0)
            {
                outHasRotation[static_cast<std::size_t>(bone)] = true;
                decodeVecStream(bone, 4, 1);
                for (int frame = 0; frame < NumFrames; ++frame)
                {
                    auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                           static_cast<std::size_t>(bone)];
                    float len = std::sqrt(sample.Rotation.X * sample.Rotation.X +
                                          sample.Rotation.Y * sample.Rotation.Y +
                                          sample.Rotation.Z * sample.Rotation.Z +
                                          sample.Rotation.W * sample.Rotation.W);
                    if (len > 0.0f)
                    {
                        sample.Rotation.X /= len;
                        sample.Rotation.Y /= len;
                        sample.Rotation.Z /= len;
                        sample.Rotation.W /= len;
                    }
                }
            }
            if ((mask & 4u) != 0)
                decodeVecStream(bone, 3, 2);
            if ((mask & 8u) != 0)
                decodeVisibility(bone);
        }

        return true;
    };

    auto scoreSamples = [&](std::vector<SuAnimationSample> const& samples,
                            std::vector<bool> const& hasRotation) {
        auto isFinite = [](float v) { return std::isfinite(v); };
        std::array<int, 4> frameSamples{0, NumFrames / 3, (NumFrames * 2) / 3, std::max(0, NumFrames - 1)};
        std::array<int, 4> boneSamples{0, NumBones / 3, (NumBones * 2) / 3, std::max(0, NumBones - 1)};
        double score = 0.0;
        for (int bone : boneSamples)
        {
            if (bone < 0 || bone >= NumBones)
                continue;
            for (int frame : frameSamples)
            {
                if (frame < 0 || frame >= NumFrames)
                    continue;
                auto const& sample = samples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                             static_cast<std::size_t>(bone)];
                bool ok = true;
                float values[] = {sample.Translation.X, sample.Translation.Y, sample.Translation.Z,
                                  sample.Rotation.X, sample.Rotation.Y, sample.Rotation.Z, sample.Rotation.W,
                                  sample.Scale.X, sample.Scale.Y, sample.Scale.Z};
                for (float v : values)
                {
                    if (!isFinite(v))
                    {
                        ok = false;
                        break;
                    }
                }
                if (!ok)
                {
                    score -= 1000.0;
                    continue;
                }

                float tAbs = std::max({std::fabs(sample.Translation.X),
                                       std::fabs(sample.Translation.Y),
                                       std::fabs(sample.Translation.Z)});
                if (tAbs < 1e5f)
                    score += 2.0;
                else
                    score -= (tAbs > 1e6f) ? 100.0 : 2.0;

                float sMin = std::min({sample.Scale.X, sample.Scale.Y, sample.Scale.Z});
                float sMax = std::max({sample.Scale.X, sample.Scale.Y, sample.Scale.Z});
                if (sMin > 0.001f && sMax < 100.0f)
                    score += 2.0;
                else
                    score -= 2.0;

                if (hasRotation[static_cast<std::size_t>(bone)])
                {
                    float len = std::sqrt(sample.Rotation.X * sample.Rotation.X +
                                          sample.Rotation.Y * sample.Rotation.Y +
                                          sample.Rotation.Z * sample.Rotation.Z +
                                          sample.Rotation.W * sample.Rotation.W);
                    if (len > 0.1f && len < 10.0f)
                        score += 2.0;
                    else
                        score -= 2.0;
                }
            }
        }
        // Dense sanity check to avoid selecting variants with huge translations or zero scales.
        double maxAbsT = 0.0;
        double minScale = std::numeric_limits<double>::infinity();
        double maxScale = 0.0;
        int nonFinite = 0;
        for (int frame = 0; frame < NumFrames; ++frame)
        {
            for (int bone = 0; bone < NumBones; ++bone)
            {
                auto const& s = samples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                        static_cast<std::size_t>(bone)];
                float values[] = {s.Translation.X, s.Translation.Y, s.Translation.Z,
                                  s.Rotation.X, s.Rotation.Y, s.Rotation.Z, s.Rotation.W,
                                  s.Scale.X, s.Scale.Y, s.Scale.Z};
                for (float v : values)
                    if (!std::isfinite(v))
                        ++nonFinite;
                double tAbs = std::max({std::fabs(static_cast<double>(s.Translation.X)),
                                        std::fabs(static_cast<double>(s.Translation.Y)),
                                        std::fabs(static_cast<double>(s.Translation.Z))});
                if (tAbs > maxAbsT)
                    maxAbsT = tAbs;
                double sMin = std::min({static_cast<double>(s.Scale.X),
                                        static_cast<double>(s.Scale.Y),
                                        static_cast<double>(s.Scale.Z)});
                double sMax = std::max({static_cast<double>(s.Scale.X),
                                        static_cast<double>(s.Scale.Y),
                                        static_cast<double>(s.Scale.Z)});
                if (sMin < minScale)
                    minScale = sMin;
                if (sMax > maxScale)
                    maxScale = sMax;
            }
        }
        if (nonFinite > 0)
            score -= 10000.0;
        if (maxAbsT > 1.0e5)
            score -= 1000.0 + std::log10(maxAbsT) * 50.0;
        if (minScale <= 0.0)
            score -= 500.0;
        else if (minScale < 0.001)
            score -= 50.0;
        if (maxScale > 100.0)
            score -= 50.0;
        return score;
    };

    if (Type == 0x06)
    {
        auto dumpHex = [&](char const* label, std::span<const std::uint8_t> data, std::size_t offset) {
            if (data.empty() || offset >= data.size())
                return;
            std::size_t count = std::min<std::size_t>(0x40u, data.size() - offset);
            std::cout << "[Type6Dump] " << label << " off=0x" << std::hex << offset << std::dec << " bytes=" << count;
            for (std::size_t i = 0; i < count; ++i)
            {
                if ((i % 16u) == 0)
                    std::cout << "\n[Type6Dump]  ";
                std::cout << std::hex << std::setw(2) << std::setfill('0')
                          << static_cast<unsigned>(data[offset + i]) << " ";
            }
            std::cout << std::dec << std::setfill(' ') << std::endl;
        };
        auto selectMaskOffset = [&](std::span<const std::uint8_t> data,
                                    std::size_t maskWords,
                                    bool bigEndian) -> std::size_t {
            std::array<std::size_t, 15> offsets{
                0u,  0x8u, 0x10u, 0x14u, 0x18u, 0x1cu, 0x20u, 0x24u,
                0x28u, 0x2cu, 0x30u, 0x34u, 0x38u, 0x3cu, 0x40u};
            std::size_t bestOffset = 0;
            int bestScore = std::numeric_limits<int>::min();
            int bestNonZero = 0;
            int bestTrans = 0;
            int bestRot = 0;
            int bestScale = 0;
            int bestVis = 0;
            for (std::size_t off : offsets)
            {
                if (off >= data.size())
                    continue;
                std::vector<std::uint32_t> masks;
                std::size_t maskBytes = 0;
                if (!ReadPackedNibbleMasks(data, off, NumBones, maskWords, bigEndian, masks, maskBytes))
                    continue;
                int nonZero = 0;
                int transCount = 0;
                int rotCount = 0;
                int scaleCount = 0;
                int visCount = 0;
                for (std::uint32_t m : masks)
                {
                    if (m != 0)
                        ++nonZero;
                    if (m & 0x1u) ++transCount;
                    if (m & 0x2u) ++rotCount;
                    if (m & 0x4u) ++scaleCount;
                    if (m & 0x8u) ++visCount;
                }
                int score = nonZero * 2 + transCount * 5 + rotCount * 2 + scaleCount * 3 + visCount;
                if (transCount == 0)
                    score -= 50;
                if (score > bestScore)
                {
                    bestScore = score;
                    bestOffset = off;
                    bestNonZero = nonZero;
                    bestTrans = transCount;
                    bestRot = rotCount;
                    bestScale = scaleCount;
                    bestVis = visCount;
                }
            }
            if (std::getenv("TYPE6_TRACE") != nullptr)
            {
                std::cout << "[Type6] maskOffset=0x" << std::hex << bestOffset << std::dec
                          << " nz=" << bestNonZero
                          << " t=" << bestTrans
                          << " r=" << bestRot
                          << " s=" << bestScale
                          << " v=" << bestVis
                          << " endian=" << (bigEndian ? "BE" : "LE")
                          << std::endl;
            }
            return bestOffset;
        };

        auto countMaskNonZero = [&](std::span<const std::uint8_t> data,
                                    std::size_t startOffset,
                                    std::size_t maskWords,
                                    bool bigEndian) -> int {
            std::vector<std::uint32_t> masks;
            std::size_t maskBytes = 0;
            if (!ReadPackedNibbleMasks(data, startOffset, NumBones, maskWords, bigEndian, masks, maskBytes))
                return 0;
            int nonZero = 0;
            for (std::uint32_t m : masks)
                if (m != 0)
                    ++nonZero;
            return nonZero;
        };

        auto findBestMaskStart = [&](std::span<const std::uint8_t> data,
                                     std::size_t anchor,
                                     std::size_t windowBefore,
                                     std::size_t windowAfter,
                                     std::size_t maskWords) -> std::optional<std::size_t> {
            if (data.empty())
                return std::nullopt;
            std::size_t start = (anchor > windowBefore) ? (anchor - windowBefore) : 0u;
            std::size_t end = std::min(anchor + windowAfter, data.size());
            if (start >= end)
                return std::nullopt;
            int bestNonZero = 0;
            std::size_t bestOffset = start;
            for (std::size_t off = start; off + maskWords * 4u <= end; ++off)
            {
                int nonZeroLE = countMaskNonZero(data, off, maskWords, false);
                int nonZeroBE = countMaskNonZero(data, off, maskWords, true);
                int nonZero = std::max(nonZeroLE, nonZeroBE);
                if (nonZero > bestNonZero)
                {
                    bestNonZero = nonZero;
                    bestOffset = off;
                    if (bestNonZero >= NumBones)
                        break;
                }
            }
            if (bestNonZero <= 0)
                return std::nullopt;
            return bestOffset;
        };

        struct Variant
        {
            bool BigEndian = false;
            std::span<const std::uint8_t> StreamData{};
            std::size_t MaskWords = 0;
            std::span<const std::uint8_t> ParamData{};
            std::size_t ParamOffset = 0;
            const char* Label = "";
        };

        double bestScore = -1e30;
        int bestMaskScore = std::numeric_limits<int>::min();
        std::vector<SuAnimationSample> bestSamples;
        SuAnimationQuantization bestQuant;
        std::vector<bool> bestHasRotation;
        bool bestEndian = Type6BigEndian;
        const char* bestLabel = "";
        int bestMaskNonZero = 0;
        std::array<std::uint32_t, 4> bestMaskSample{};

        std::size_t maskWordsDefault = ComputeType6MaskWordCount(NumBones, NumUvBones, NumFloatStreams);
        std::size_t maskWordsNoFloat = ComputeType6MaskWordCount(NumBones, NumUvBones, 0);
        std::span<const std::uint8_t> fullData = fullSpan.empty() ? blockSpan : fullSpan;
        std::size_t paramAbs = static_cast<std::size_t>(std::max(0, Type6ParamDataOffset));
        std::size_t paramRel = 0;
        bool hasRel = Type6BlockEnd > Type6BlockStart &&
                      Type6ParamDataOffset >= 0 &&
                      static_cast<std::size_t>(Type6ParamDataOffset) >= Type6BlockStart &&
                      static_cast<std::size_t>(Type6ParamDataOffset) < Type6BlockEnd;
        if (hasRel)
            paramRel = static_cast<std::size_t>(Type6ParamDataOffset) - Type6BlockStart;

        std::optional<std::size_t> bestMaskStart = findBestMaskStart(fullData, Type6Anchor, 0x400, 0x4000, maskWordsDefault);
        std::optional<std::size_t> bestMaskStartNoFloat = findBestMaskStart(fullData, Type6Anchor, 0x400, 0x4000, maskWordsNoFloat);
        std::span<const std::uint8_t> scanSpan = bestMaskStart ? fullData.subspan(*bestMaskStart) : std::span<const std::uint8_t>{};
        std::span<const std::uint8_t> scanSpanNoFloat = bestMaskStartNoFloat ? fullData.subspan(*bestMaskStartNoFloat) : std::span<const std::uint8_t>{};

        auto withBestMaskOffset = [&](std::span<const std::uint8_t> base,
                                      std::size_t maskWords,
                                      bool bigEndian) -> std::span<const std::uint8_t> {
            if (base.empty())
                return base;
            std::size_t off = selectMaskOffset(base, maskWords, bigEndian);
            if (off >= base.size())
                return base;
            return base.subspan(off);
        };

        std::array<Variant, 12> variants = {{
            {Type6BigEndian, withBestMaskOffset(blockSpan, maskWordsDefault, Type6BigEndian), maskWordsDefault, fullData, paramAbs, "abs+mask"},
            {!Type6BigEndian, withBestMaskOffset(blockSpan, maskWordsDefault, !Type6BigEndian), maskWordsDefault, fullData, paramAbs, "abs+mask"},
            {Type6BigEndian, withBestMaskOffset(blockSpan, maskWordsNoFloat, Type6BigEndian), maskWordsNoFloat, fullData, paramAbs, "abs+nofloat"},
            {!Type6BigEndian, withBestMaskOffset(blockSpan, maskWordsNoFloat, !Type6BigEndian), maskWordsNoFloat, fullData, paramAbs, "abs+nofloat"},
            {Type6BigEndian, hasRel ? withBestMaskOffset(blockSpan, maskWordsDefault, Type6BigEndian) : std::span<const std::uint8_t>{}, maskWordsDefault, blockSpan, paramRel, "rel+mask"},
            {!Type6BigEndian, hasRel ? withBestMaskOffset(blockSpan, maskWordsDefault, !Type6BigEndian) : std::span<const std::uint8_t>{}, maskWordsDefault, blockSpan, paramRel, "rel+mask"},
            {Type6BigEndian, withBestMaskOffset(scanSpan, maskWordsDefault, Type6BigEndian), maskWordsDefault, fullData, paramAbs, "scan+mask"},
            {!Type6BigEndian, withBestMaskOffset(scanSpan, maskWordsDefault, !Type6BigEndian), maskWordsDefault, fullData, paramAbs, "scan+mask"},
            {Type6BigEndian, hasRel ? withBestMaskOffset(scanSpan, maskWordsDefault, Type6BigEndian) : std::span<const std::uint8_t>{}, maskWordsDefault, blockSpan, paramRel, "scan+relmask"},
            {!Type6BigEndian, hasRel ? withBestMaskOffset(scanSpan, maskWordsDefault, !Type6BigEndian) : std::span<const std::uint8_t>{}, maskWordsDefault, blockSpan, paramRel, "scan+relmask"},
            {Type6BigEndian, withBestMaskOffset(scanSpanNoFloat, maskWordsNoFloat, Type6BigEndian), maskWordsNoFloat, fullData, paramAbs, "scan+nofloat"},
            {!Type6BigEndian, withBestMaskOffset(scanSpanNoFloat, maskWordsNoFloat, !Type6BigEndian), maskWordsNoFloat, fullData, paramAbs, "scan+nofloat"},
        }};

        for (auto const& variant : variants)
        {
            if (variant.ParamData.empty())
                continue;
            if (variant.ParamOffset >= variant.ParamData.size())
                continue;
            std::vector<SuAnimationSample> tempSamples;
            SuAnimationQuantization tempQuant;
            std::vector<bool> tempHasRotation;
            int tempMaskNonZero = 0;
            std::array<std::uint32_t, 4> tempMaskSample{};
            int tempMaskScore = 0;
            if (!decodeType6UsUs(variant.BigEndian, variant.StreamData, variant.MaskWords, variant.ParamData,
                                 variant.ParamOffset, tempSamples, tempQuant, tempHasRotation,
                                 tempMaskNonZero, tempMaskSample, tempMaskScore))
            {
                continue;
            }
            if (tempMaskNonZero == 0)
                continue;
            double score = scoreSamples(tempSamples, tempHasRotation);
            if (score > bestScore || (score == bestScore && tempMaskScore > bestMaskScore))
            {
                bestScore = score;
                bestMaskScore = tempMaskScore;
                bestSamples = std::move(tempSamples);
                bestQuant = std::move(tempQuant);
                bestHasRotation = std::move(tempHasRotation);
                bestEndian = variant.BigEndian;
                bestLabel = variant.Label;
                bestMaskNonZero = tempMaskNonZero;
                bestMaskSample = tempMaskSample;
            }
        }

        if (std::getenv("TYPE6_TRACE") != nullptr && bestLabel && *bestLabel)
        {
            std::cout << "[Type6] bestVariant=" << bestLabel
                      << " endian=" << (bestEndian ? "BE" : "LE")
                      << " score=" << bestScore
                      << " maskNonZero=" << bestMaskNonZero
                      << std::endl;
        }

        if (std::getenv("TYPE6_DUMP") != nullptr)
        {
            std::size_t dumpMaskOff = 0;
            if (bestLabel && std::strncmp(bestLabel, "scan", 4) == 0 && bestMaskStart)
                dumpMaskOff = *bestMaskStart;
            dumpHex("block", fullData, dumpMaskOff);
            dumpHex("param", fullData, paramAbs);
        }

        if (bestSamples.empty())
            return false;
        Samples = std::move(bestSamples);
        Quantization = std::move(bestQuant);
        SamplesDecoded = true;
        Type6BigEndian = bestEndian;
        Type6DebugMasksLogged = true;
        Type6MaskNonZero = bestMaskNonZero;
        Type6MaskSample = bestMaskSample;
        return true;
    }

    bool smallHeader = (Type == 0x07 || Type == 0x0A);
    int frameIndexSize = (Type == 0x07 || Type == 0x0A) ? 1 : 2;
    int factorSize = 0;
    if (Type == 0x08)
        factorSize = 4;
    else if (Type == 0x09 || Type == 0x0A)
        factorSize = 1;
    else
        factorSize = 2;

    auto buildOrders = []() {
        std::array<int, 4> base{0, 1, 2, 3};
        std::vector<std::array<int, 4>> orders;
        orders.reserve(24);
        std::sort(base.begin(), base.end());
        do
        {
            orders.push_back(base);
        } while (std::next_permutation(base.begin(), base.end()));
        return orders;
    };


    std::vector<std::array<int, 4>> orders{{{0, 1, 2, 3}}};
    std::array<int, 1> bitWidths{16};
    std::array<int, 1> factorModes{0};
    double bestScore = -1e30;
    std::vector<SuAnimationSample> bestSamples;
    SuAnimationQuantization bestQuant;
    std::vector<bool> bestHasRotation;
    bool bestEndian = Type6BigEndian;
    std::size_t maskWords = ComputeType6MaskWordCount(NumBones, NumUvBones, NumFloatStreams);

    bool trace = std::getenv("TYPE6_TRACE") != nullptr;
    auto tryDecode = [&](bool currentBigEndian) {
        std::vector<std::uint32_t> masks;
        std::size_t maskBytes = 0;
        if (!ReadPackedNibbleMasks(blockSpan, 0, NumBones, maskWords, currentBigEndian, masks, maskBytes))
            return;
        if (trace || debugDump)
            std::cout << "[Type6] endian=" << (currentBigEndian ? "BE" : "LE")
                      << " maskBytes=" << maskBytes
                      << " blockBytes=" << blockSpan.size()
                      << " paramData=" << Type6ParamDataOffset
                      << " paramGpu=" << (Type6ParamDataIsGpu ? 1 : 0)
                      << " dataSize=" << Type6DataSize << std::endl;
        if (trace || debugDump)
            std::cout.flush();
        if (!Type6DebugMasksLogged)
        {
            Type6DebugMasksLogged = true;
            int nonZero = 0;
            for (std::uint32_t m : masks)
                if (m != 0)
                    ++nonZero;
            Type6MaskNonZero = nonZero;
            for (std::size_t i = 0; i < Type6MaskSample.size(); ++i)
                Type6MaskSample[i] = (i < masks.size()) ? masks[i] : 0u;
        }
        if (debugDump && !masks.empty())
        {
            std::cout << "[Type6Dump] mask0=0x" << std::hex << masks[0] << std::dec
                      << " anchor=" << Type6Anchor
                      << " blockStart=" << Type6BlockStart
                      << " blockEnd=" << Type6BlockEnd
                      << " paramOffset=" << Type6ParamDataOffset
                      << " paramGpu=" << (Type6ParamDataIsGpu ? 1 : 0)
                      << std::endl;
        }

        auto decodeWithOrder = [&](std::array<int, 4> const& order,
                                   int bitWidth,
                                   bool msbPacking,
                                   int factorMode,
                                   std::vector<SuAnimationSample>& outSamples,
                                   SuAnimationQuantization& outQuant,
                                   std::vector<bool>& outHasRotation) {
            initSamples(outSamples);
            outQuant.Resize(static_cast<std::size_t>(NumBones));
            outHasRotation.assign(static_cast<std::size_t>(NumBones), false);

            Type6Reader reader{blockSpan, currentBigEndian, maskBytes};
            Type6Reader paramReader{};
            std::size_t paramOffset = static_cast<std::size_t>(std::max(0, Type6ParamDataOffset));
            if (!fullSpan.empty())
            {
                Type6Reader paramFull{fullSpan, currentBigEndian, paramOffset};
                paramReader = paramFull;
            }
            else
            {
                Type6Reader paramLocal{blockSpan, currentBigEndian, 0};
                paramReader = paramLocal;
            }
            paramReader.Offset = Align4(paramReader.Offset);
            int debugBone = -1;
            bool debugLogged = false;
            if (std::getenv("DECODE_DEBUG") != nullptr)
            {
                for (int i = 0; i < NumBones; ++i)
                {
                    if (static_cast<std::size_t>(i) < masks.size() && masks[static_cast<std::size_t>(i)] != 0)
                    {
                        debugBone = i;
                        break;
                    }
                }
            }
            auto readFrameIndices = [&](std::size_t count) {
                std::vector<int> values;
                values.reserve(count);
                for (std::size_t i = 0; i < count; ++i)
                {
                    int v = 0;
                    if (frameIndexSize == 1)
                        v = reader.ReadU8();
                    else
                        v = static_cast<int>(reader.ReadU16());
                    values.push_back(v);
                }
                return values;
            };

            std::size_t debugStreamCount = 0;
            bool debugDump = std::getenv("TYPE6_DUMP") != nullptr;
            auto decodeStream = [&](int boneIndex, int channel, int componentCount) {
                Type6StreamHeader header = ReadType6HeaderInlineSafe(reader, smallHeader,
                                                               static_cast<std::size_t>(NumFrames),
                                                               static_cast<std::size_t>(frameIndexSize),
                                                               static_cast<std::size_t>(factorSize));
                std::size_t frameCount = header.NumFrames == 0 ? static_cast<std::size_t>(NumFrames)
                                                          : header.NumFrames;
                std::size_t keyCount = header.NumKeys;
                if (frameCount == 0 || frameCount > static_cast<std::size_t>(NumFrames))
                    return;
                if (keyCount == 0 || keyCount > 4096u)
                    return;
                if (trace && debugStreamCount < 8)
                {
                    ++debugStreamCount;
                    std::cout << "[Type6] stream#" << debugStreamCount
                              << " bone=" << boneIndex
                              << " ch=" << channel
                              << " comps=" << componentCount
                              << " frames=" << frameCount
                              << " keys=" << keyCount
                              << " inlineOff=" << reader.Offset
                              << " paramOff=" << paramReader.Offset
                              << std::endl;
                }

                std::size_t keyTimesCount = keyCount;
                if (!reader.Has(keyTimesCount * static_cast<std::size_t>(frameIndexSize)))
                    return;

                auto keyTimes = readFrameIndices(keyTimesCount);
                if ((trace || debugDump) && debugStreamCount <= 1)
                    std::cout << "[Type6]  keyTimesRead off=" << reader.Offset << std::endl;
                reader.Offset = Align2(reader.Offset);

                paramReader.Offset = Align4(paramReader.Offset);
                float minimum = 0.0f;
                float delta = 0.0f;
                if (paramReader.Has(8))
                {
                    minimum = paramReader.ReadFloat();
                    delta = paramReader.ReadFloat();
                }
                if ((trace || debugDump) && debugStreamCount <= 1)
                    std::cout << "[Type6]  minDeltaRead off=" << paramReader.Offset << std::endl;

                std::size_t samplesPerKey = static_cast<std::size_t>(componentCount) * 3u;
                std::size_t sampleCount = keyCount * samplesPerKey + static_cast<std::size_t>(componentCount);
                std::vector<short> samples;
                samples.resize(sampleCount);
                std::size_t byteCount = sampleCount * sizeof(short);
                if (paramReader.Offset + byteCount <= paramReader.Data.size())
                {
                    for (std::size_t i = 0; i < sampleCount; ++i)
                    {
                        std::uint16_t raw = paramReader.ReadU16();
                        samples[i] = static_cast<std::int16_t>(raw);
                    }
                    paramReader.Offset = Align4(paramReader.Offset);
                }
                else
                {
                    paramReader.Offset = paramReader.Data.size();
                }
                if (debugDump && debugStreamCount == 1)
                {
                    std::cout << "[Type6Dump] bone=" << boneIndex
                              << " ch=" << channel
                              << " keys=" << keyCount
                              << " min=" << minimum
                              << " delta=" << delta
                              << " keyTimes0=" << (keyTimes.empty() ? -1 : keyTimes[0])
                              << " keyTimesLast=" << (keyTimes.empty() ? -1 : keyTimes.back())
                              << " sample0=" << (samples.empty() ? 0 : samples[0])
                              << " sample1=" << (samples.size() > 1 ? samples[1] : 0)
                              << " sample2=" << (samples.size() > 2 ? samples[2] : 0)
                              << std::endl;
                }
                if (trace && debugStreamCount <= 1)
                    std::cout << "[Type6]  samplesRead off=" << paramReader.Offset << std::endl;

                auto sampleValue = [&](int keyIndex, int sampleSet, int component) -> float {
                    if (keyCount == 0)
                        return 0.0f;
                    if (keyIndex < 0)
                        keyIndex = 0;
                    if (keyIndex >= static_cast<int>(keyCount))
                        keyIndex = static_cast<int>(keyCount) - 1;
                    int idx = keyIndex * static_cast<int>(samplesPerKey) +
                              sampleSet * componentCount + component;
                    if (idx < 0 || static_cast<std::size_t>(idx) >= samples.size())
                        return 0.0f;
                    float normalized = static_cast<float>(samples[static_cast<std::size_t>(idx)]) / 32767.0f;
                    return minimum + delta * normalized;
                };
                auto sampleValueNext = [&](int keyIndex, int component) -> float {
                    int idx = keyIndex * static_cast<int>(samplesPerKey) + componentCount * 3 + component;
                    if (idx < 0 || static_cast<std::size_t>(idx) >= samples.size())
                        return 0.0f;
                    float normalized = static_cast<float>(samples[static_cast<std::size_t>(idx)]) / 32767.0f;
                    return minimum + delta * normalized;
                };

                if (std::getenv("DECODE_DEBUG") != nullptr &&
                    boneIndex == debugBone &&
                    !debugLogged)
                {
                    debugLogged = true;
                    int midFrame = NumFrames > 1 ? (NumFrames / 2) : 0;
                    auto evalFrame = [&](int frameIndex) {
                        int localFrame = frameIndex;
                        if (frameCount > 0)
                            localFrame = localFrame % static_cast<int>(frameCount);
                        if (localFrame < 0)
                            localFrame = 0;
                        int k = 0;
                        if (!keyTimes.empty())
                        {
                            int idx = static_cast<int>(keyTimes.size()) - 1;
                            for (std::size_t i = 0; i < keyTimes.size(); ++i)
                            {
                                if (localFrame < static_cast<int>(keyTimes[i]))
                                {
                                    idx = static_cast<int>(i);
                                    break;
                                }
                            }
                            k = idx;
                            if (k >= static_cast<int>(keyCount))
                                k = static_cast<int>(keyCount) - 1;
                        }
                        float t = 0.0f;
                        if (!keyTimes.empty() && k >= 0 && static_cast<std::size_t>(k) < keyTimes.size())
                        {
                            int end = static_cast<int>(keyTimes[static_cast<std::size_t>(k)]);
                            int start = (k == 0) ? 0 : static_cast<int>(keyTimes[static_cast<std::size_t>(k - 1)]);
                            int denom = end - start;
                            if (denom > 0)
                                t = static_cast<float>(localFrame - start) / static_cast<float>(denom);
                        }
                        float ts = t * t;
                        float tc = ts * t;
                        float p0 = sampleValue(k, 0, 0);
                        float p1 = sampleValue(k, 1, 0);
                        float p2 = sampleValue(k, 2, 0);
                        float p3 = (k + 1 < static_cast<int>(keyCount)) ? sampleValue(k + 1, 0, 0)
                                                                        : sampleValueNext(k, 0);
                        float value = (((p3 - p2) - p1) - p0) * tc + p2 * ts + p1 * t + p0;
                        return std::tuple<float, float, float, float, float>(t, p0, p1, p2, p3);
                    };
                    auto [t0, p0a, p1a, p2a, p3a] = evalFrame(0);
                    auto [t1, p0b, p1b, p2b, p3b] = evalFrame(midFrame);
                    std::cout << "[DecodeDbg] bone=" << boneIndex
                              << " ch=" << channel
                              << " mask=0x" << std::hex
                              << (static_cast<std::size_t>(boneIndex) < masks.size() ? masks[static_cast<std::size_t>(boneIndex)] : 0u)
                              << std::dec
                              << " frames=" << frameCount
                              << " keys=" << keyCount
                              << " key0=" << (keyTimes.empty() ? -1 : keyTimes.front())
                              << " keyLast=" << (keyTimes.empty() ? -1 : keyTimes.back())
                              << " min=" << minimum
                              << " delta=" << delta
                              << " s0=" << (samples.size() > 0 ? samples[0] : 0)
                              << " s1=" << (samples.size() > 1 ? samples[1] : 0)
                              << " s2=" << (samples.size() > 2 ? samples[2] : 0)
                              << " t0=" << t0 << " p0=" << p0a << " p1=" << p1a << " p2=" << p2a << " p3=" << p3a
                              << " tMid=" << t1 << " p0m=" << p0b << " p1m=" << p1b << " p2m=" << p2b << " p3m=" << p3b
                              << std::endl;
                }

                for (int frame = 0; frame < NumFrames; ++frame)
                {
                    int localFrame = frame;
                    if (frameCount > 0)
                        localFrame = localFrame % static_cast<int>(frameCount);
                    if (localFrame < 0)
                        localFrame = 0;

                    int k = 0;
                    if (!keyTimes.empty())
                    {
                        int idx = static_cast<int>(keyTimes.size()) - 1;
                        for (std::size_t i = 0; i < keyTimes.size(); ++i)
                        {
                            if (localFrame < static_cast<int>(keyTimes[i]))
                            {
                                idx = static_cast<int>(i);
                                break;
                            }
                        }
                        k = idx;
                        if (k >= static_cast<int>(keyCount))
                            k = static_cast<int>(keyCount) - 1;
                    }

                    float t = 0.0f;
                    if (!keyTimes.empty() && k >= 0 && static_cast<std::size_t>(k) < keyTimes.size())
                    {
                        int end = static_cast<int>(keyTimes[static_cast<std::size_t>(k)]);
                        int start = (k == 0) ? 0 : static_cast<int>(keyTimes[static_cast<std::size_t>(k - 1)]);
                        int denom = end - start;
                        if (denom > 0)
                            t = static_cast<float>(localFrame - start) / static_cast<float>(denom);
                    }

                    float ts = t * t;
                    float tc = ts * t;
                    auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                              static_cast<std::size_t>(boneIndex)];

                    for (int component = 0; component < componentCount; ++component)
                    {
                        float p0 = sampleValue(k, 0, component);
                        float p1 = sampleValue(k, 1, component);
                        float p2 = sampleValue(k, 2, component);
                        float p3 = (k + 1 < static_cast<int>(keyCount)) ? sampleValue(k + 1, 0, component)
                                                                        : sampleValueNext(k, component);
                        float value = (((p3 - p2) - p1) - p0) * tc + p2 * ts + p1 * t + p0;
                        if (channel == 0)
                        {
                            if (component == 0) sample.Translation.X = value;
                            if (component == 1) sample.Translation.Y = value;
                            if (component == 2) sample.Translation.Z = value;
                        }
                        else if (channel == 1)
                        {
                            if (component == 0) sample.Rotation.X = value;
                            if (component == 1) sample.Rotation.Y = value;
                            if (component == 2) sample.Rotation.Z = value;
                            if (component == 3) sample.Rotation.W = value;
                        }
                        else if (channel == 2)
                        {
                            if (component == 0) sample.Scale.X = value;
                            if (component == 1) sample.Scale.Y = value;
                            if (component == 2) sample.Scale.Z = value;
                        }
                    }
                }

                for (int component = 0; component < componentCount; ++component)
                {
                    auto& range = (channel == 0) ? outQuant.Translation[static_cast<std::size_t>(boneIndex)][component]
                                 : (channel == 1) ? outQuant.Rotation[static_cast<std::size_t>(boneIndex)][component]
                                 : outQuant.Scale[static_cast<std::size_t>(boneIndex)][component];
                    range.Minimum = minimum;
                    range.Delta = delta;
                    range.Valid = true;
                }
            };

            for (int bone = 0; bone < NumBones; ++bone)
            {
                std::uint32_t maskBits = masks[static_cast<std::size_t>(bone)];
                for (int channel : order)
                {
                    if (channel == 0 && (maskBits & 0x1u))
                    {
                        decodeStream(bone, 0, 3);
                    }
                    else if (channel == 1 && (maskBits & 0x2u))
                    {
                        outHasRotation[static_cast<std::size_t>(bone)] = true;
                        decodeStream(bone, 1, 4);
                    }
                    else if (channel == 2 && (maskBits & 0x4u))
                    {
                        decodeStream(bone, 2, 3);
                    }
                    else if (channel == 3 && (maskBits & 0x8u))
                    {
                        Type6StreamHeader header = ReadType6HeaderInlineSafe(reader, smallHeader,
                                                                   static_cast<std::size_t>(NumFrames),
                                                                   static_cast<std::size_t>(frameIndexSize),
                                                                   static_cast<std::size_t>(factorSize));
                        std::size_t frameCount = header.NumFrames == 0 ? static_cast<std::size_t>(NumFrames)
                                                                  : header.NumFrames;
                        std::size_t keyCount = header.NumKeys;
                        if (frameCount == 0 || keyCount == 0)
                            continue;
                        std::size_t keyTimesCount = keyCount;
                        if (!reader.Has(keyTimesCount * static_cast<std::size_t>(frameIndexSize)))
                            continue;
                        auto keyTimes = readFrameIndices(keyTimesCount);
                        reader.Offset = Align2(reader.Offset);

                        for (int frame = 0; frame < NumFrames; ++frame)
                        {
                            int localFrame = frameCount > 0 ? frame % static_cast<int>(frameCount) : frame;
                            if (localFrame < 0)
                                localFrame = 0;
                            int idx = static_cast<int>(keyTimes.size()) - 1;
                            for (std::size_t i = 0; i < keyTimes.size(); ++i)
                            {
                                if (localFrame < static_cast<int>(keyTimes[i]))
                                {
                                    idx = static_cast<int>(i);
                                    break;
                                }
                            }
                            if (idx >= static_cast<int>(keyCount))
                                idx = static_cast<int>(keyCount) - 1;
                            auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                                      static_cast<std::size_t>(bone)];
                            sample.Visible = (idx % 2) == 0;
                        }
                    }
                }
            }

            for (int bone = 0; bone < NumBones; ++bone)
            {
                if (!outHasRotation[static_cast<std::size_t>(bone)])
                    continue;
                for (int frame = 0; frame < NumFrames; ++frame)
                {
                    auto& sample = outSamples[static_cast<std::size_t>(frame) * static_cast<std::size_t>(NumBones) +
                                              static_cast<std::size_t>(bone)];
                    float len = std::sqrt(sample.Rotation.X * sample.Rotation.X +
                                          sample.Rotation.Y * sample.Rotation.Y +
                                          sample.Rotation.Z * sample.Rotation.Z +
                                          sample.Rotation.W * sample.Rotation.W);
                    if (len > 1e-6f)
                    {
                        float inv = 1.0f / len;
                        sample.Rotation.X *= inv;
                        sample.Rotation.Y *= inv;
                        sample.Rotation.Z *= inv;
                        sample.Rotation.W *= inv;
                    }
                }
            }
        };

        for (auto const& order : orders)
        {
            for (int bitWidth : bitWidths)
            {
                for (int factorMode : factorModes)
                {
                    std::vector<SuAnimationSample> tempSamples;
                    SuAnimationQuantization tempQuant;
                    std::vector<bool> tempHasRotation;
                    decodeWithOrder(order, bitWidth, false, factorMode, tempSamples, tempQuant, tempHasRotation);
                    double score = scoreSamples(tempSamples, tempHasRotation);
                    if (score > bestScore)
                    {
                        bestScore = score;
                        bestSamples = std::move(tempSamples);
                        bestQuant = std::move(tempQuant);
                        bestHasRotation = std::move(tempHasRotation);
                        bestEndian = currentBigEndian;
                    }
                }
            }
        }
    };

    tryDecode(Type6BigEndian);
    tryDecode(!Type6BigEndian);

    if (bestSamples.empty())
        return false;

    Samples = std::move(bestSamples);
    Quantization = std::move(bestQuant);

    SamplesDecoded = true;
    Type6BigEndian = bestEndian;
    return true;
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
                if (type != 0 && type != 1 && type != 4 && (type < 6 || type > 10))
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
