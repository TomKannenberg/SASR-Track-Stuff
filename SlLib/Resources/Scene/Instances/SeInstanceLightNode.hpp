#pragma once

#include "SlLib/Math/Vector.hpp"
#include "SeInstanceEntityNode.hpp"

namespace SlLib::Resources::Scene::Instances {

class SeInstanceLightNode final : public ::SlLib::Resources::Scene::SeInstanceEntityNode
{
public:
    enum class SeLightType : int
    {
        Ambient = 0,
        Directional = 1,
        Point = 2,
        Spot = 3,
    };

    SeInstanceLightNode();
    ~SeInstanceLightNode() override;

    int LightDataFlags = 0;
    SeLightType LightType = SeLightType::Ambient;

    float SpecularMultiplier = 1.0f;
    float IntensityMultiplier = 1.0f;
    SlLib::Math::Vector3 Color{};

    float InnerRadius = 0.0f;
    float OuterRadius = 0.0f;
    float Falloff = 0.0f;

    float InnerConeAngle = 0.0f;
    float OuterConeAngle = 0.0f;
};

} // namespace SlLib::Resources::Scene::Instances
