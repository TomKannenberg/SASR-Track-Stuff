#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "EnumHelpers.hpp"

namespace SlLib::Enums {

enum class AudioVolumeShape
{
    Sphere,
    Box,
    Line,
};

inline constexpr auto allAudioVolumeShapes() noexcept -> std::array<AudioVolumeShape, 3>
{
    return {AudioVolumeShape::Sphere, AudioVolumeShape::Box, AudioVolumeShape::Line};
}

std::string toString(AudioVolumeShape shape);

template <>
std::optional<AudioVolumeShape> fromString<AudioVolumeShape>(std::string_view text);

inline std::optional<AudioVolumeShape> fromStringAudioVolumeShape(std::string_view text)
{
    return fromString<AudioVolumeShape>(text);
}

} // namespace SlLib::Enums
