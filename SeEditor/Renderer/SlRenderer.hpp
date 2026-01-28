#pragma once

#include "EditorFramebuffer.hpp"
#include "SlRenderBuffers.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>
#include <utility>
#include <glad/glad.h>
#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Forest {
struct SuRenderTextureResource;
}

namespace SeEditor::Renderer {

class SlRenderer final
{
public:
    SlRenderer();

    struct ForestCpuMesh
    {
        // Interleaved per-vertex floats:
        //   pos(3), normal(3), uv(2), weights(4), indices(4 as floats)
        // Unskinned meshes should use weights=(1,0,0,0), indices=(0,0,0,0).
        std::vector<float> Vertices;
        std::vector<std::uint32_t> Indices;
        std::shared_ptr<SeEditor::Forest::SuRenderTextureResource> Texture;
        SlLib::Math::Matrix4x4 Model{};
        bool Skinned = false;
        std::vector<int> BoneMatrixIndices;
        std::vector<SlLib::Math::Matrix4x4> BoneInverseMatrices;
        std::vector<SlLib::Math::Matrix4x4> BonePalette;
        int ForestIndex = -1;
        int TreeIndex = -1;
        int BranchIndex = -1;
    };

    struct DebugLine
    {
        SlLib::Math::Vector3 From{};
        SlLib::Math::Vector3 To{};
        SlLib::Math::Vector3 Color{1.0f, 1.0f, 1.0f};
    };

    void SetForestMeshes(std::vector<ForestCpuMesh> meshes);
    void SetDrawForestMeshes(bool enable) { _drawForestMeshes = enable; }
    void UpdateForestBonePalettes(std::vector<std::vector<SlLib::Math::Matrix4x4>> palettes);

    void Initialize();
    void Render();
    void Resize(int width, int height);
    void SetCollisionMesh(std::vector<SlLib::Math::Vector3> vertices,
                          std::vector<std::array<int, 3>> triangles);
    void SetForestBoxes(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes);
    void SetDrawForestBoxes(bool enable) { _drawForestBoxes = enable; }
    void SetTriggerBoxes(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes);
    void SetDrawTriggerBoxes(bool enable) { _drawTriggerBoxes = enable; }
    void SetDebugLines(std::vector<DebugLine> lines);
    void SetDrawDebugLines(bool enable) { _drawDebugLines = enable; }
    void SetBoneLines(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> bones);
    void SetDrawBoneLines(bool enable) { _drawBoneLines = enable; }
    void SetOrbitCamera(float yaw, float pitch, float distance, SlLib::Math::Vector3 target);
    void SetDrawCollisionMesh(bool enable) { _drawCollisionMesh = enable; }
    void SetDrawOriginAxes(bool enable) { _drawOriginAxes = enable; }

    std::size_t DebugForestMeshCount() const { return _forestCpuMeshes.size(); }
    std::size_t DebugForestSkinnedCount() const;
    bool DebugForestDirty() const { return _forestDirty; }
    double DebugFrameMs() const { return _frameMs; }
    double DebugForestMs() const { return _forestMs; }
    double DebugForestSetupMs() const { return _forestSetupMs; }
    double DebugForestBonesMs() const { return _forestBonesMs; }
    double DebugForestDrawMs() const { return _forestDrawMs; }
    double DebugGpuWaitMs() const { return _gpuWaitMs; }
    double DebugCollisionMs() const { return _collisionMs; }
    double DebugForestBoxesMs() const { return _forestBoxesMs; }
    double DebugTriggerBoxesMs() const { return _triggerBoxesMs; }
    double DebugDebugLinesMs() const { return _debugLinesMs; }
    double DebugOriginAxesMs() const { return _originAxesMs; }
    double DebugPrimitiveMs() const { return _primitiveMs; }
    double DebugForestUploadMs() const { return _forestUploadMs; }

private:
    struct ForestGpuMesh
    {
        GLuint Vao = 0;
        GLuint Vbo = 0;
        GLuint Ebo = 0;
        GLuint Texture = 0;
        std::size_t IndexCount = 0;
        std::size_t CpuIndex = 0;
    };

    EditorFramebuffer _framebuffer;
    SlRenderBuffers _buffers;
    std::vector<SlLib::Math::Vector3> _collisionVertices;
    std::vector<std::array<int, 3>> _collisionTriangles;
    SlLib::Math::Vector3 _collisionCenter{0.0f, 0.0f, 0.0f};
    float _collisionRadius = 1.0f;
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> _forestBoxes;
    bool _drawForestBoxes = false;
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> _triggerBoxes;
    bool _drawTriggerBoxes = false;
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> _boneLines;
    bool _drawBoneLines = false;
    std::vector<DebugLine> _debugLines;
    std::vector<ForestCpuMesh> _forestCpuMeshes;
    std::vector<ForestGpuMesh> _forestGpuMeshes;
    std::unordered_map<const void*, GLuint> _forestTextureCache;
    bool _forestDirty = false;
    bool _drawForestMeshes = false;
    float _orbitYaw = 0.6f;
    float _orbitPitch = 0.35f;
    float _orbitDistance = 10.0f;
    SlLib::Math::Vector3 _orbitTarget{0.0f, 0.5f, 0.0f};
    bool _drawCollisionMesh = true;
    bool _drawDebugLines = false;
    bool _drawOriginAxes = true;
    double _frameMs = 0.0;
    double _forestMs = 0.0;
    double _forestSetupMs = 0.0;
    double _forestBonesMs = 0.0;
    double _forestDrawMs = 0.0;
    double _gpuWaitMs = 0.0;
    double _collisionMs = 0.0;
    double _forestBoxesMs = 0.0;
    double _triggerBoxesMs = 0.0;
    double _debugLinesMs = 0.0;
    double _originAxesMs = 0.0;
    double _primitiveMs = 0.0;
    double _forestUploadMs = 0.0;
};

} // namespace SeEditor::Renderer
