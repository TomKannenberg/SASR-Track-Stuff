#pragma once

#include "SlLib/Math/Vector.hpp"

namespace SlLib::Resources::Model {

struct SlModelInstanceData
{
    Math::Matrix4x4 InstanceWorldMatrix{};
    Math::Matrix4x4 InstanceBindMatrix{};
    Math::Matrix4x4 WorldMatrix{};
    Math::Matrix4x4 CullMatrix{};
    int RenderMask = 0;
    int LodGroupFlags = 0;
    short Visibility = 0;
};

} // namespace SlLib::Resources::Model
