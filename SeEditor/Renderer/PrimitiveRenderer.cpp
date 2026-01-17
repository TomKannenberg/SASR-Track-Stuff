#include "PrimitiveRenderer.hpp"

#include <glad/glad.h>
#include <imgui.h>

#include <SlLib/Math/Vector.hpp>

#include <array>
#include <chrono>
#include <cmath>
#include <iostream>
#include <vector>

namespace SeEditor::Renderer {

namespace {

struct PrimitiveLine
{
    SlLib::Math::Vector3 From;
    SlLib::Math::Vector3 To;
    SlLib::Math::Vector3 Color;
};

std::vector<PrimitiveLine> s_lines;
bool s_active = false;

GLuint s_vao = 0;
GLuint s_vbo = 0;
GLuint s_program = 0;
SlLib::Math::Matrix4x4 s_view{};
SlLib::Math::Matrix4x4 s_proj{};

SlLib::Math::Matrix4x4 MakePerspective(float fovYRadians, float aspect, float zNear, float zFar)
{
    float f = 1.0f / std::tan(fovYRadians / 2.0f);
    SlLib::Math::Matrix4x4 m{};
    m(0, 0) = f / aspect;
    m(1, 1) = f;
    m(2, 2) = (zFar + zNear) / (zNear - zFar);
    m(2, 3) = (2.0f * zFar * zNear) / (zNear - zFar);
    m(3, 2) = -1.0f;
    return m;
}

SlLib::Math::Matrix4x4 MakeLookAt(SlLib::Math::Vector3 eye,
                                  SlLib::Math::Vector3 target,
                                  SlLib::Math::Vector3 up)
{
    using SlLib::Math::Vector3;
    Vector3 f = SlLib::Math::normalize(target - eye);
    Vector3 s = SlLib::Math::normalize(SlLib::Math::cross(f, up));
    Vector3 u = SlLib::Math::cross(s, f);

    SlLib::Math::Matrix4x4 m{};
    m(0, 0) = s.X; m(0, 1) = s.Y; m(0, 2) = s.Z;
    m(1, 0) = u.X; m(1, 1) = u.Y; m(1, 2) = u.Z;
    m(2, 0) = -f.X; m(2, 1) = -f.Y; m(2, 2) = -f.Z;
    m(3, 3) = 1.0f;

    // translation
    m(0, 3) = -SlLib::Math::dot(s, eye);
    m(1, 3) = -SlLib::Math::dot(u, eye);
    m(2, 3) = SlLib::Math::dot(f, eye);
    return m;
}

void EnsureGlResources()
{
    if (s_program != 0)
        return;

    const char* vsSource = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vColor;
void main() {
    vColor = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
})";

    const char* fsSource = R"(#version 330 core
in vec3 vColor;
out vec4 FragColor;
void main() {
    FragColor = vec4(vColor, 1.0);
})";

    auto compileShader = [](GLenum type, const char* src) -> GLuint {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);
        GLint ok = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
        if (!ok)
        {
            char log[1024];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::cerr << "[PrimitiveRenderer] Shader compile error: " << log << std::endl;
        }
        return shader;
    };

    GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
    GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);

    s_program = glCreateProgram();
    glAttachShader(s_program, vs);
    glAttachShader(s_program, fs);
    glLinkProgram(s_program);
    GLint linked = 0;
    glGetProgramiv(s_program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        char log[1024];
        glGetProgramInfoLog(s_program, sizeof(log), nullptr, log);
        std::cerr << "[PrimitiveRenderer] Program link error: " << log << std::endl;
    }
    glDeleteShader(vs);
    glDeleteShader(fs);

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);
}

std::array<float, 16> ToColumnMajor(SlLib::Math::Matrix4x4 const& m)
{
    std::array<float, 16> out{};
    for (int row = 0; row < 4; ++row)
        for (int col = 0; col < 4; ++col)
            out[col * 4 + row] = m(row, col);
    return out;
}

} // namespace

void PrimitiveRenderer::BeginPrimitiveScene() noexcept
{
    s_lines.clear();
    s_active = true;
}

void PrimitiveRenderer::EndPrimitiveScene() noexcept
{
    if (!s_active)
        return;

    if (s_lines.empty())
    {
        s_active = false;
        return;
    }

    EnsureGlResources();

    // Build vertex buffer (position + color)
    std::vector<float> verts;
    verts.reserve(s_lines.size() * 2 * 6);
    for (auto const& line : s_lines)
    {
        verts.push_back(line.From.X);
        verts.push_back(line.From.Y);
        verts.push_back(line.From.Z);
        verts.push_back(line.Color.X);
        verts.push_back(line.Color.Y);
        verts.push_back(line.Color.Z);

        verts.push_back(line.To.X);
        verts.push_back(line.To.Y);
        verts.push_back(line.To.Z);
        verts.push_back(line.Color.X);
        verts.push_back(line.Color.Y);
        verts.push_back(line.Color.Z);
    }

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(verts.size() * sizeof(float)),
                 verts.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    glUseProgram(s_program);

    // Fallback camera if caller didn't provide one this frame
    if (s_proj(0, 0) == 0.0f && s_proj(1, 1) == 0.0f)
    {
        ImVec2 display = ImGui::GetIO().DisplaySize;
        float aspect = display.y > 0.0f ? display.x / display.y : 16.0f / 9.0f;
        s_proj = MakePerspective(60.0f * 3.14159265f / 180.0f, aspect, 0.1f, 200000.0f);
    }
    if (s_view(3, 3) == 0.0f)
    {
        s_view = MakeLookAt({3.5f, 3.0f, 3.5f}, {0.0f, 0.5f, 0.0f}, {0.0f, 1.0f, 0.0f});
    }

    auto mvp = ToColumnMajor(SlLib::Math::Multiply(s_proj, s_view));

    GLint loc = glGetUniformLocation(s_program, "uMVP");
    if (loc >= 0)
        glUniformMatrix4fv(loc, 1, GL_FALSE, mvp.data());

    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(s_lines.size() * 2));

    glBindVertexArray(0);
    glUseProgram(0);

    s_lines.clear();
    s_active = false;
}

void PrimitiveRenderer::DrawLine(SlLib::Math::Vector3 const& from,
                                 SlLib::Math::Vector3 const& to,
                                 SlLib::Math::Vector3 const& color) noexcept
{
    if (!s_active)
        return;

    s_lines.push_back({from, to, color});
}

void PrimitiveRenderer::SetCamera(SlLib::Math::Matrix4x4 const& view,
                                  SlLib::Math::Matrix4x4 const& proj) noexcept
{
    s_view = view;
    s_proj = proj;
}

} // namespace SeEditor::Renderer
