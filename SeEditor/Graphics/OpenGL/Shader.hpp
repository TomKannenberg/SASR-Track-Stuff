#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>

#include <SlLib/Math/Vector.hpp>

namespace SeEditor::Graphics::OpenGL {

class Shader final
{
public:
    Shader(std::filesystem::path vertexPath, std::filesystem::path fragmentPath);

    void SetMatrix4(std::string const& name, SlLib::Math::Matrix4x4 const& matrix);
    void SetVector3(std::string const& name, SlLib::Math::Vector4 const& vector);
    void SetVector3(std::string const& name, SlLib::Math::Vector3 const& vector);
    void SetInt(std::string const& name, int value);

    void Bind();
    void Unbind();

private:
    int locateUniform(std::string const& name);
    std::string loadSource(std::filesystem::path const& path);

    std::filesystem::path _vertexPath;
    std::filesystem::path _fragmentPath;
    std::unordered_map<std::string, int> _locations;
    bool _bound = false;
};

} // namespace SeEditor::Graphics::OpenGL
