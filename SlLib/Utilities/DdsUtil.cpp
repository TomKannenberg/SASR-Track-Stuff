#include "DdsUtil.hpp"

#include <algorithm>
#include <cstring>

namespace SlLib::Utilities {

namespace {

constexpr std::uint32_t kDdsMagic = 0x20534444; // "DDS "
constexpr std::uint32_t kFourCC = 0x00000004;

std::uint32_t ReadU32(std::span<const std::uint8_t> data, std::size_t offset)
{
    if (offset + 4 > data.size())
        return 0;
    std::uint32_t value = 0;
    std::memcpy(&value, data.data() + offset, sizeof(value));
    return value;
}

bool FourCCEquals(std::uint32_t value, char a, char b, char c, char d)
{
    return value == (static_cast<std::uint32_t>(a) |
                     (static_cast<std::uint32_t>(b) << 8) |
                     (static_cast<std::uint32_t>(c) << 16) |
                     (static_cast<std::uint32_t>(d) << 24));
}

} // namespace

bool DdsUtil::Parse(std::span<const std::uint8_t> data, DdsInfo& info, std::string& error)
{
    if (data.size() < 0x80)
    {
        error = "DDS header too small.";
        return false;
    }

    if (ReadU32(data, 0) != kDdsMagic)
    {
        error = "Missing DDS magic.";
        return false;
    }

    std::uint32_t headerSize = ReadU32(data, 4);
    if (headerSize != 124)
    {
        error = "Unexpected DDS header size.";
        return false;
    }

    info.Height = ReadU32(data, 12);
    info.Width = ReadU32(data, 16);
    info.MipLevels = std::max<std::uint32_t>(1, ReadU32(data, 28));

    std::uint32_t caps2 = ReadU32(data, 0x70);
    info.IsCubemap = (caps2 & 0xFE00) != 0;

    std::uint32_t pfFlags = ReadU32(data, 0x50);
    std::uint32_t fourCC = ReadU32(data, 0x54);

    info.Format = DdsFormat::Unknown;
    if (pfFlags & kFourCC)
    {
        if (FourCCEquals(fourCC, 'D', 'X', 'T', '1'))
            info.Format = DdsFormat::BC1;
        else if (FourCCEquals(fourCC, 'D', 'X', 'T', '3'))
            info.Format = DdsFormat::BC2;
        else if (FourCCEquals(fourCC, 'D', 'X', 'T', '5'))
            info.Format = DdsFormat::BC3;
    }
    else
    {
        std::uint32_t rgbBits = ReadU32(data, 0x58);
        if (rgbBits == 32)
            info.Format = DdsFormat::RGBA8;
    }

    if (info.Format == DdsFormat::Unknown)
    {
        error = "Unsupported DDS format.";
        return false;
    }

    return true;
}

std::size_t DdsUtil::ComputeTotalSize(DdsInfo const& info)
{
    std::size_t total = 0;
    std::uint32_t width = std::max(1u, info.Width);
    std::uint32_t height = std::max(1u, info.Height);
    std::uint32_t mips = std::max(1u, info.MipLevels);
    std::size_t faces = info.IsCubemap ? 6 : 1;

    for (std::size_t face = 0; face < faces; ++face)
    {
        std::uint32_t w = width;
        std::uint32_t h = height;
        for (std::uint32_t mip = 0; mip < mips; ++mip)
        {
            if (info.Format == DdsFormat::RGBA8)
            {
                total += static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 4;
            }
            else
            {
                std::size_t blockSize = (info.Format == DdsFormat::BC1) ? 8 : 16;
                std::size_t bw = (std::max(1u, w) + 3) / 4;
                std::size_t bh = (std::max(1u, h) + 3) / 4;
                total += bw * bh * blockSize;
            }

            w = std::max(1u, w >> 1);
            h = std::max(1u, h >> 1);
        }
    }

    return total + 0x80; // header size
}

} // namespace SlLib::Utilities
