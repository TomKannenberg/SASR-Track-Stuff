#include "SlRenderer.hpp"

#include "PrimitiveRenderer.hpp"
#include "SeEditor/Forest/ForestTypes.hpp"
#include "SlLib/Utilities/DdsUtil.hpp"

#include <iostream>
#include <array>
#include <glad/glad.h>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifndef GL_COMPRESSED_RGBA_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT3_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#endif
#ifndef GL_COMPRESSED_RGBA_S3TC_DXT5_EXT
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif

namespace SeEditor::Renderer {

SlRenderer::SlRenderer() = default;

std::size_t SlRenderer::DebugForestSkinnedCount() const
{
    std::size_t count = 0;
    for (auto const& mesh : _forestCpuMeshes)
    {
        if (mesh.Skinned && !mesh.BonePalette.empty())
            ++count;
    }
    return count;
}

namespace {

GLuint g_forestProgram = 0;

std::array<float, 16> ToColumnMajor(SlLib::Math::Matrix4x4 const& m)
{
    std::array<float, 16> out{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            out[col * 4 + row] = m(row, col);
    return out;
}

GLuint CompileShader(GLenum type, const char* src)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    GLint ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::cerr << "[SlRenderer] Shader compile error: " << log << std::endl;
    }
    return shader;
}

GLuint EnsureForestProgram()
{
    if (g_forestProgram != 0)
        return g_forestProgram;

    const char* vsSource = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aBoneWeights;
layout(location = 4) in vec4 aBoneIndices;
uniform mat4 uVP;
uniform mat4 uView;
uniform mat4 uModel;
uniform int uUseSkinning;
uniform int uBoneCount;
const int MAX_BONES = 256;
uniform mat4 uBones[MAX_BONES];
out vec3 vNormal;
out vec2 vUv;
void main() {
    vec4 worldPos = uModel * vec4(aPos, 1.0);
    vec3 worldNormal = mat3(uModel) * aNormal;
    if (uUseSkinning != 0) {
        ivec4 idx = ivec4(aBoneIndices + vec4(0.5));
        int hi = (uBoneCount > 0) ? min(uBoneCount - 1, MAX_BONES - 1) : 0;
        idx = clamp(idx, ivec4(0), ivec4(hi));
        mat4 skin =
            uBones[idx.x] * aBoneWeights.x +
            uBones[idx.y] * aBoneWeights.y +
            uBones[idx.z] * aBoneWeights.z +
            uBones[idx.w] * aBoneWeights.w;
        worldPos = skin * vec4(aPos, 1.0);
        worldNormal = mat3(skin) * aNormal;
    }
    vNormal = mat3(uView) * normalize(worldNormal);
    vUv = vec2(aUv.x, 1.0 - aUv.y);
    gl_Position = uVP * vec4(worldPos.xyz, 1.0);
})";

    const char* fsSource = R"(#version 330 core
in vec3 vNormal;
in vec2 vUv;
out vec4 FragColor;
uniform sampler2D uTexture;
void main() {
    vec3 lightDir = normalize(vec3(0.4, 0.8, 0.2));
    float ndotl = max(dot(normalize(vNormal), lightDir), 0.15);
    vec4 tex = texture(uTexture, vUv);
    FragColor = vec4(tex.rgb * ndotl, tex.a);
})";

    GLuint vs = CompileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fsSource);
    g_forestProgram = glCreateProgram();
    glAttachShader(g_forestProgram, vs);
    glAttachShader(g_forestProgram, fs);
    glLinkProgram(g_forestProgram);
    GLint linked = 0;
    glGetProgramiv(g_forestProgram, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        glGetProgramInfoLog(g_forestProgram, sizeof(log), nullptr, log);
        std::cerr << "[SlRenderer] Forest program link error: " << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    return g_forestProgram;
}

GLuint UploadFallbackTexture()
{
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    std::uint8_t white[4] = {255, 255, 255, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint UploadDdsTexture(std::vector<std::uint8_t> const& dds)
{
    if (dds.size() < 0x80)
        return 0;

    SlLib::Utilities::DdsInfo info;
    std::string error;
    if (!SlLib::Utilities::DdsUtil::Parse(dds, info, error))
        return 0;

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);

    GLenum internalFormat = GL_RGBA8;
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    bool compressed = false;
    if (info.Format == SlLib::Utilities::DdsFormat::BC1)
    {
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
        compressed = true;
    }
    else if (info.Format == SlLib::Utilities::DdsFormat::BC2)
    {
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
        compressed = true;
    }
    else if (info.Format == SlLib::Utilities::DdsFormat::BC3)
    {
        internalFormat = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
        compressed = true;
    }

    std::size_t offset = 0x80;
    std::uint32_t width = std::max(1u, info.Width);
    std::uint32_t height = std::max(1u, info.Height);
    std::uint32_t mips = std::max(1u, info.MipLevels);

    for (std::uint32_t mip = 0; mip < mips; ++mip)
    {
        std::size_t size = 0;
        if (compressed)
        {
            std::size_t blockSize = (info.Format == SlLib::Utilities::DdsFormat::BC1) ? 8 : 16;
            std::size_t bw = (std::max(1u, width) + 3) / 4;
            std::size_t bh = (std::max(1u, height) + 3) / 4;
            size = bw * bh * blockSize;
            if (offset + size > dds.size())
                break;
            glCompressedTexImage2D(GL_TEXTURE_2D, mip, internalFormat,
                                   static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                                   0, static_cast<GLsizei>(size), dds.data() + offset);
        }
        else
        {
            size = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
            if (offset + size > dds.size())
                break;
            glTexImage2D(GL_TEXTURE_2D, mip, internalFormat,
                         static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                         0, format, type, dds.data() + offset);
        }

        offset += size;
        width = std::max(1u, width >> 1);
        height = std::max(1u, height >> 1);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mips > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    return tex;
}

} // namespace

void SlRenderer::Initialize()
{
    _framebuffer.Initialize(1920, 1080);
    _buffers.Initialize();
}

void SlRenderer::SetForestMeshes(std::vector<ForestCpuMesh> meshes)
{
    _forestCpuMeshes = std::move(meshes);
    _forestDirty = true;
}

void SlRenderer::UpdateForestBonePalettes(std::vector<std::vector<SlLib::Math::Matrix4x4>> palettes)
{
    std::size_t n = std::min(palettes.size(), _forestCpuMeshes.size());
    for (std::size_t i = 0; i < n; ++i)
        _forestCpuMeshes[i].BonePalette = std::move(palettes[i]);
}

void SlRenderer::Render()
{
    if (!_framebuffer.IsValid())
        return;

    _framebuffer.Bind();

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.07f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Camera that frames either the collision mesh or the debug cube.
    ImVec2 display = ImGui::GetIO().DisplaySize;
    float aspect = display.y > 0.0f ? display.x / display.y : 16.0f / 9.0f;
    auto makePerspective = [](float fovY, float aspectRatio, float zNear, float zFar) {
        using SlLib::Math::Matrix4x4;
        float f = 1.0f / std::tan(fovY / 2.0f);
        Matrix4x4 m{};
        m(0, 0) = f / aspectRatio;
        m(1, 1) = f;
        m(2, 2) = (zFar + zNear) / (zNear - zFar);
        m(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
        m(3, 2) = -1.0f;
        return m;
    };
    auto makeLookAt = [](SlLib::Math::Vector3 eye, SlLib::Math::Vector3 target, SlLib::Math::Vector3 up) {
        using SlLib::Math::Vector3;
        Vector3 f = SlLib::Math::normalize(target - eye);
        Vector3 s = SlLib::Math::normalize(SlLib::Math::cross(f, up));
        Vector3 u = SlLib::Math::cross(s, f);

        SlLib::Math::Matrix4x4 m{};
        m(0, 0) = s.X; m(0, 1) = s.Y; m(0, 2) = s.Z; m(0, 3) = -SlLib::Math::dot(s, eye);
        m(1, 0) = u.X; m(1, 1) = u.Y; m(1, 2) = u.Z; m(1, 3) = -SlLib::Math::dot(u, eye);
        m(2, 0) = -f.X; m(2, 1) = -f.Y; m(2, 2) = -f.Z; m(2, 3) = SlLib::Math::dot(f, eye);
        m(3, 3) = 1.0f;
        return m;
    };

    SlLib::Math::Vector3 eye = _orbitTarget;
    float cp = std::cos(_orbitPitch);
    float sp = std::sin(_orbitPitch);
    float cy = std::cos(_orbitYaw);
    float sy = std::sin(_orbitYaw);
    SlLib::Math::Vector3 forward{cp * cy, sp, cp * sy};
    SlLib::Math::Vector3 target = eye + forward;

    auto proj = makePerspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 200000.0f);
    auto view = makeLookAt(eye, target, {0.0f, 1.0f, 0.0f});
    PrimitiveRenderer::SetCamera(view, proj);

    PrimitiveRenderer::BeginPrimitiveScene();

    // Draw simple axes
    if (_drawOriginAxes)
    {
        using SlLib::Math::Vector3;
        PrimitiveRenderer::DrawLine({0.0f, 0.0f, 0.0f}, {1.5f, 0.0f, 0.0f}, {1.0f, 0.1f, 0.1f});
        PrimitiveRenderer::DrawLine({0.0f, 0.0f, 0.0f}, {0.0f, 1.5f, 0.0f}, {0.1f, 1.0f, 0.1f});
        PrimitiveRenderer::DrawLine({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.5f}, {0.1f, 0.1f, 1.0f});
    }

    if (_drawCollisionMesh && !_collisionTriangles.empty())
    {
        SlLib::Math::Vector3 col = {0.4f, 0.9f, 0.9f};
        for (auto const& tri : _collisionTriangles)
        {
            auto const& a = _collisionVertices[static_cast<std::size_t>(tri[0])];
            auto const& b = _collisionVertices[static_cast<std::size_t>(tri[1])];
            auto const& c = _collisionVertices[static_cast<std::size_t>(tri[2])];
            PrimitiveRenderer::DrawLine(a, b, col);
            PrimitiveRenderer::DrawLine(b, c, col);
            PrimitiveRenderer::DrawLine(c, a, col);
        }
    }
    // Forest boxes
    if (_drawForestBoxes && !_forestBoxes.empty())
    {
        SlLib::Math::Vector3 col = {0.2f, 0.9f, 0.2f};
        const std::array<std::pair<int, int>, 12> edges = {{
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        }};

        for (auto const& box : _forestBoxes)
        {
            auto const& min = box.first;
            auto const& max = box.second;
            std::array<SlLib::Math::Vector3, 8> corners = {
                SlLib::Math::Vector3{min.X, min.Y, min.Z},
                SlLib::Math::Vector3{max.X, min.Y, min.Z},
                SlLib::Math::Vector3{max.X, max.Y, min.Z},
                SlLib::Math::Vector3{min.X, max.Y, min.Z},
                SlLib::Math::Vector3{min.X, min.Y, max.Z},
                SlLib::Math::Vector3{max.X, min.Y, max.Z},
                SlLib::Math::Vector3{max.X, max.Y, max.Z},
                SlLib::Math::Vector3{min.X, max.Y, max.Z}
            };

            auto drawEdge = [&](int a, int b) {
                PrimitiveRenderer::DrawLine(corners[a], corners[b], col);
            };

            for (auto const& edge : edges)
                drawEdge(edge.first, edge.second);
        }
    }

    // Trigger phantom boxes (kill/respawn zones)
    if (_drawTriggerBoxes && !_triggerBoxes.empty())
    {
        SlLib::Math::Vector3 col = {0.9f, 0.2f, 0.2f};
        const std::array<std::pair<int, int>, 12> edges = {{
            {0, 1}, {1, 2}, {2, 3}, {3, 0},
            {4, 5}, {5, 6}, {6, 7}, {7, 4},
            {0, 4}, {1, 5}, {2, 6}, {3, 7}
        }};

        for (auto const& box : _triggerBoxes)
        {
            auto const& min = box.first;
            auto const& max = box.second;
            std::array<SlLib::Math::Vector3, 8> corners = {
                SlLib::Math::Vector3{min.X, min.Y, min.Z},
                SlLib::Math::Vector3{max.X, min.Y, min.Z},
                SlLib::Math::Vector3{max.X, max.Y, min.Z},
                SlLib::Math::Vector3{min.X, max.Y, min.Z},
                SlLib::Math::Vector3{min.X, min.Y, max.Z},
                SlLib::Math::Vector3{max.X, min.Y, max.Z},
                SlLib::Math::Vector3{max.X, max.Y, max.Z},
                SlLib::Math::Vector3{min.X, max.Y, max.Z}
            };

            auto drawEdge = [&](int a, int b) {
                PrimitiveRenderer::DrawLine(corners[a], corners[b], col);
            };

            for (auto const& edge : edges)
                drawEdge(edge.first, edge.second);
        }
    }

    if (_drawDebugLines && !_debugLines.empty())
    {
        for (auto const& line : _debugLines)
            PrimitiveRenderer::DrawLine(line.From, line.To, line.Color);
    }

    if (_drawBoneLines && !_boneLines.empty())
    {
        SlLib::Math::Vector3 boneColor{0.9f, 0.6f, 0.1f};
        for (auto const& bone : _boneLines)
            PrimitiveRenderer::DrawLine(bone.first, bone.second, boneColor);
    }

    if (_forestDirty)
    {
        for (auto& mesh : _forestGpuMeshes)
        {
            if (mesh.Ebo) glDeleteBuffers(1, &mesh.Ebo);
            if (mesh.Vbo) glDeleteBuffers(1, &mesh.Vbo);
            if (mesh.Vao) glDeleteVertexArrays(1, &mesh.Vao);
        }
        _forestGpuMeshes.clear();

        for (auto const& pair : _forestTextureCache)
            glDeleteTextures(1, &pair.second);
        _forestTextureCache.clear();

        GLuint fallback = UploadFallbackTexture();
        for (std::size_t cpuIndex = 0; cpuIndex < _forestCpuMeshes.size(); ++cpuIndex)
        {
            auto const& cpu = _forestCpuMeshes[cpuIndex];
            ForestGpuMesh gpu;
            gpu.IndexCount = cpu.Indices.size();

            glGenVertexArrays(1, &gpu.Vao);
            glGenBuffers(1, &gpu.Vbo);
            glGenBuffers(1, &gpu.Ebo);

            glBindVertexArray(gpu.Vao);
            glBindBuffer(GL_ARRAY_BUFFER, gpu.Vbo);
            glBufferData(GL_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(cpu.Vertices.size() * sizeof(float)),
                         cpu.Vertices.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu.Ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         static_cast<GLsizeiptr>(cpu.Indices.size() * sizeof(std::uint32_t)),
                         cpu.Indices.data(), GL_STATIC_DRAW);

            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 16 * sizeof(float), reinterpret_cast<void*>(0));
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                                  reinterpret_cast<void*>(3 * sizeof(float)));
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                                  reinterpret_cast<void*>(6 * sizeof(float)));
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                                  reinterpret_cast<void*>(8 * sizeof(float)));
            glEnableVertexAttribArray(4);
            glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 16 * sizeof(float),
                                  reinterpret_cast<void*>(12 * sizeof(float)));

            glBindVertexArray(0);

            GLuint texture = fallback;
            if (cpu.Texture)
            {
                auto key = static_cast<const void*>(cpu.Texture.get());
                auto it = _forestTextureCache.find(key);
                if (it != _forestTextureCache.end())
                {
                    texture = it->second;
                }
                else if (!cpu.Texture->ImageData.empty())
                {
                    GLuint created = UploadDdsTexture(cpu.Texture->ImageData);
                    if (created != 0)
                        texture = created;
                    _forestTextureCache.emplace(key, texture);
                }
            }
            gpu.Texture = texture;
            gpu.CpuIndex = cpuIndex;
            _forestGpuMeshes.push_back(gpu);
        }
        _forestDirty = false;
    }

    if (std::getenv("RENDER_PRE_DEBUG") != nullptr)
    {
        static auto s_last = std::chrono::steady_clock::now();
        auto now = std::chrono::steady_clock::now();
        if (now - s_last >= std::chrono::seconds(1))
        {
            s_last = now;
            std::size_t gpuCount = _forestGpuMeshes.size();
            std::size_t cpuCount = _forestCpuMeshes.size();
            std::size_t boneCount = 0;
            bool skinned = false;
            if (!_forestCpuMeshes.empty())
            {
                auto const& cpu = _forestCpuMeshes.front();
                skinned = cpu.Skinned && !cpu.BonePalette.empty();
                boneCount = cpu.BonePalette.size();
                if (skinned && !cpu.BonePalette.empty())
                {
                    auto const& m = cpu.BonePalette.front();
                    std::cout << "[RenderPre] firstBone m00=" << m(0, 0)
                              << " m03=" << m(0, 3)
                              << " m13=" << m(1, 3)
                              << " m23=" << m(2, 3)
                              << std::endl;
                }
            }
            std::cout << "[RenderPre] gpuMeshes=" << gpuCount
                      << " cpuMeshes=" << cpuCount
                      << " dirty=" << (_forestDirty ? 1 : 0)
                      << " firstSkinned=" << (skinned ? 1 : 0)
                      << " firstBones=" << boneCount
                      << std::endl;
        }
    }

    if (_drawForestMeshes && !_forestGpuMeshes.empty())
    {
        GLuint program = EnsureForestProgram();
        glUseProgram(program);
        auto vp = ToColumnMajor(SlLib::Math::Multiply(proj, view));
        auto viewMat = ToColumnMajor(view);
        GLint locVp = glGetUniformLocation(program, "uVP");
        GLint locView = glGetUniformLocation(program, "uView");
        GLint locModel = glGetUniformLocation(program, "uModel");
        GLint locUseSkinning = glGetUniformLocation(program, "uUseSkinning");
        GLint locBoneCount = glGetUniformLocation(program, "uBoneCount");
        GLint locBones = glGetUniformLocation(program, "uBones");
        if (locVp >= 0)
            glUniformMatrix4fv(locVp, 1, GL_FALSE, vp.data());
        if (locView >= 0)
            glUniformMatrix4fv(locView, 1, GL_FALSE, viewMat.data());
        glUniform1i(glGetUniformLocation(program, "uTexture"), 0);

        for (auto const& mesh : _forestGpuMeshes)
        {
            SlLib::Math::Matrix4x4 model{};
            bool skinned = false;
            std::size_t boneCount = 0;
            if (mesh.CpuIndex < _forestCpuMeshes.size())
            {
                auto const& cpu = _forestCpuMeshes[mesh.CpuIndex];
                model = cpu.Model;
                skinned = cpu.Skinned && !cpu.BonePalette.empty();
                boneCount = cpu.BonePalette.size();
                if (boneCount > 256)
                    boneCount = 256;
                if (skinned && locBones >= 0)
                {
                    std::vector<float> boneFloats;
                    boneFloats.reserve(boneCount * 16);
                    for (std::size_t i = 0; i < boneCount; ++i)
                    {
                        auto cm = ToColumnMajor(cpu.BonePalette[i]);
                        boneFloats.insert(boneFloats.end(), cm.begin(), cm.end());
                    }
                    glUniformMatrix4fv(locBones, static_cast<GLsizei>(boneCount), GL_FALSE, boneFloats.data());
                }
            }

            if (locModel >= 0)
            {
                auto modelCol = ToColumnMajor(model);
                glUniformMatrix4fv(locModel, 1, GL_FALSE, modelCol.data());
            }
            if (locUseSkinning >= 0)
                glUniform1i(locUseSkinning, skinned ? 1 : 0);
            if (locBoneCount >= 0)
                glUniform1i(locBoneCount, static_cast<GLint>(boneCount));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, mesh.Texture);
            glBindVertexArray(mesh.Vao);
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh.IndexCount), GL_UNSIGNED_INT, nullptr);
        }

        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        glUseProgram(0);
    }

    PrimitiveRenderer::EndPrimitiveScene();
    _framebuffer.Unbind();
}

void SlRenderer::Resize(int width, int height)
{
    _framebuffer.Resize(width, height);
}

void SlRenderer::SetCollisionMesh(std::vector<SlLib::Math::Vector3> vertices,
                                  std::vector<std::array<int, 3>> triangles)
{
    _collisionVertices = std::move(vertices);
    _collisionTriangles = std::move(triangles);

    _collisionCenter = {0.0f, 0.0f, 0.0f};
    _collisionRadius = 1.0f;
    if (!_collisionVertices.empty())
    {
        SlLib::Math::Vector3 min = _collisionVertices[0];
        SlLib::Math::Vector3 max = _collisionVertices[0];
        for (auto const& v : _collisionVertices)
        {
            min.X = std::min(min.X, v.X);
            min.Y = std::min(min.Y, v.Y);
            min.Z = std::min(min.Z, v.Z);
            max.X = std::max(max.X, v.X);
            max.Y = std::max(max.Y, v.Y);
            max.Z = std::max(max.Z, v.Z);
        }
        _collisionCenter = {(min.X + max.X) * 0.5f, (min.Y + max.Y) * 0.5f, (min.Z + max.Z) * 0.5f};
        SlLib::Math::Vector3 extents = {max.X - min.X, max.Y - min.Y, max.Z - min.Z};
        _collisionRadius = std::max({extents.X, extents.Y, extents.Z}) * 0.5f;
    }
}

void SlRenderer::SetForestBoxes(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes)
{
    _forestBoxes = std::move(boxes);
}

void SlRenderer::SetTriggerBoxes(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> boxes)
{
    _triggerBoxes = std::move(boxes);
}

void SlRenderer::SetDebugLines(std::vector<DebugLine> lines)
{
    _debugLines = std::move(lines);
}

void SlRenderer::SetBoneLines(std::vector<std::pair<SlLib::Math::Vector3, SlLib::Math::Vector3>> bones)
{
    _boneLines = std::move(bones);
}

void SlRenderer::SetOrbitCamera(float yaw, float pitch, float distance, SlLib::Math::Vector3 target)
{
    _orbitYaw = yaw;
    _orbitPitch = std::clamp(pitch, -1.4f, 1.4f);
    _orbitDistance = std::max(0.0f, distance);
    _orbitTarget = target;
}

} // namespace SeEditor::Renderer
