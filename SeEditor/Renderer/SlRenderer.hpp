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
        std::vector<float> Vertices; // pos(3) + normal(3) + uv(2)
        std::vector<std::uint32_t> Indices;
        std::shared_ptr<SeEditor::Forest::SuRenderTextureResource> Texture;
    };

    void SetForestMeshes(std::vector<ForestCpuMesh> meshes);
    void SetDrawForestMeshes(bool enable) { _drawForestMeshes = enable; }

    void Initialize();
    void Render();
    void Resize(int width, int height);
    void SetCollisionMesh(std::vector<SlLib::Math::Vector3> vertices,
                          std::vector<std::array<int, 3>> triangles);
    void SetForestBoxes(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes);
    void SetDrawForestBoxes(bool enable) { _drawForestBoxes = enable; }
    void SetOrbitCamera(float yaw, float pitch, float distance, SlLib::Math::Vector3 target);
    void SetDrawCollisionMesh(bool enable) { _drawCollisionMesh = enable; }

private:
    struct ForestGpuMesh
    {
        GLuint Vao = 0;
        GLuint Vbo = 0;
        GLuint Ebo = 0;
        GLuint Texture = 0;
        std::size_t IndexCount = 0;
    };

    EditorFramebuffer _framebuffer;
    SlRenderBuffers _buffers;
    std::vector<SlLib::Math::Vector3> _collisionVertices;
    std::vector<std::array<int, 3>> _collisionTriangles;
    SlLib::Math::Vector3 _collisionCenter{0.0f, 0.0f, 0.0f};
    float _collisionRadius = 1.0f;
    std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> _forestBoxes;
    bool _drawForestBoxes = false;
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
};

} // namespace SeEditor::Renderer
