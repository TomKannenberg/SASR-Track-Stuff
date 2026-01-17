#pragma once

#include "../../SlLib/Math/Vector.hpp"
#include "../../SlLib/Utilities/MathUtils.hpp"
#include "../Renderer/Buffers/ConstantBufferViewProjection.hpp"

namespace SeEditor::Editor {

class SceneCamera
{
public:
    SceneCamera();

    static SceneCamera& Active();
    static void SetActive(SceneCamera& camera);

    float Fov = 60.0f * SlLib::Utilities::Deg2Rad;
    float AspectRatio = 16.0f / 9.0f;
    float Near = 0.1f;
    float Far = 20000.0f;
    SlLib::Math::Vector3 Position{};
    SlLib::Math::Vector3 Rotation{};

    SlLib::Math::Matrix4x4 const& GetView() const;
    SlLib::Math::Matrix4x4 const& GetProjection() const;

    void RecomputeMatrixData();
    bool IsOnFrustum() const;
    void TranslateLocal(SlLib::Math::Vector3 delta);

    struct Plane
    {
        Plane() = default;
        Plane(SlLib::Math::Vector3 point, SlLib::Math::Vector3 normal);

        SlLib::Math::Vector3 Normal{};
        float Distance = 0.0f;

        float GetSignedDistanceToPlane(SlLib::Math::Vector3 point) const;
    };

    struct Frustum
    {
        Plane TopFace;
        Plane BottomFace;
        Plane RightFace;
        Plane LeftFace;
        Plane FarFace;
        Plane NearFace;
    };

    Frustum CameraFrustum;
    SeEditor::Renderer::Buffers::ConstantBufferViewProjection MatrixData;

private:
    SlLib::Math::Matrix4x4 _inverseRotation{};
    static SceneCamera* _active;
};

} // namespace SeEditor::Editor
