#include "AudioVolumeShape.hpp"

namespace SlLib::Enums {

std::string toString(AudioVolumeShape shape)
{
    switch (shape) {
    case AudioVolumeShape::Sphere:
        return "Sphere";
    case AudioVolumeShape::Box:
        return "Box";
    case AudioVolumeShape::Line:
        return "Line";
    }

    return "Unknown";
}

template <>
std::optional<AudioVolumeShape> fromString<AudioVolumeShape>(std::string_view text)
{
    return fromStringFromAll<AudioVolumeShape>(allAudioVolumeShapes, text);
}

} // namespace SlLib::Enums
