#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace SlLib::Utilities {

enum class DdsFormat
{
    Unknown,
    BC1,
    BC2,
    BC3,
    RGBA8
};

struct DdsInfo
{
    std::uint32_t Width = 0;
    std::uint32_t Height = 0;
    std::uint32_t MipLevels = 1;
    bool IsCubemap = false;
    DdsFormat Format = DdsFormat::Unknown;
};

class DdsUtil
{
public:
    static bool Parse(std::span<const std::uint8_t> data, DdsInfo& info, std::string& error);
    static std::size_t ComputeTotalSize(DdsInfo const& info);
};

} // namespace SlLib::Utilities
