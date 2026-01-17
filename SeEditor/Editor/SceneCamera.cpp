#include "SceneCamera.hpp"

namespace {

SlLib::Math::Matrix4x4 makeIdentity()
{
    SlLib::Math::Matrix4x4 result{};
    result(0, 0) = 1.0f;
    result(1, 1) = 1.0f;
    result(2, 2) = 1.0f;
    result(3, 3) = 1.0f;
    return result;
}

} // namespace

namespace SeEditor::Editor {

SceneCamera* SceneCamera::_active = nullptr;

SceneCamera::SceneCamera()
{
    if (_active == nullptr)
    {
        _active = this;
    }

    MatrixData.View = makeIdentity();
    MatrixData.Projection = makeIdentity();
    MatrixData.ViewInverse = makeIdentity();
    MatrixData.ViewProjection = makeIdentity();
}

SceneCamera& SceneCamera::Active()
{
    if (_active == nullptr)
    {
        static SceneCamera instance;
        _active = &instance;
    }

    return *_active;
}

void SceneCamera::SetActive(SceneCamera& camera)
{
    _active = &camera;
}

SlLib::Math::Matrix4x4 const& SceneCamera::GetView() const
{
    return MatrixData.View;
}

SlLib::Math::Matrix4x4 const& SceneCamera::GetProjection() const
{
    return MatrixData.Projection;
}

void SceneCamera::RecomputeMatrixData()
{
    MatrixData.View = makeIdentity();
    MatrixData.Projection = makeIdentity();
    MatrixData.ViewInverse = makeIdentity();
    MatrixData.ViewProjection = makeIdentity();
}

bool SceneCamera::IsOnFrustum() const
{
    return true;
}

void SceneCamera::TranslateLocal(SlLib::Math::Vector3 delta)
{
    Position.X += delta.X;
    Position.Y += delta.Y;
    Position.Z += delta.Z;
}

SceneCamera::Plane::Plane(SlLib::Math::Vector3 point, SlLib::Math::Vector3 normal)
    : Normal(SlLib::Math::normalize(normal))
    , Distance(SlLib::Math::dot(normal, point))
{
}

float SceneCamera::Plane::GetSignedDistanceToPlane(SlLib::Math::Vector3 point) const
{
    return SlLib::Math::dot(Normal, point) - Distance;
}

} // namespace SeEditor::Editor
