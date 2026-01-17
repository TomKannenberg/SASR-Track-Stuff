#include "Shader.hpp"

#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace SeEditor::Graphics::OpenGL {

Shader::Shader(std::filesystem::path vertexPath, std::filesystem::path fragmentPath)
    : _vertexPath(std::move(vertexPath))
    , _fragmentPath(std::move(fragmentPath))
{
    if (!std::filesystem::exists(_vertexPath))
        throw std::runtime_error("Vertex shader not found: " + _vertexPath.string());

    if (!std::filesystem::exists(_fragmentPath))
        throw std::runtime_error("Fragment shader not found: " + _fragmentPath.string());

    loadSource(_vertexPath);
    loadSource(_fragmentPath);
}

void Shader::SetMatrix4(std::string const& name, SlLib::Math::Matrix4x4 const&)
{
    locateUniform(name);
}

void Shader::SetVector3(std::string const& name, SlLib::Math::Vector4 const&)
{
    locateUniform(name);
}

void Shader::SetVector3(std::string const& name, SlLib::Math::Vector3 const&)
{
    locateUniform(name);
}

void Shader::SetInt(std::string const& name, int)
{
    locateUniform(name);
}

void Shader::Bind()
{
    if (_bound)
        return;
    _bound = true;
}

void Shader::Unbind()
{
    if (!_bound)
        return;
    _bound = false;
}

int Shader::locateUniform(std::string const& name)
{
    auto it = _locations.find(name);
    if (it != _locations.end())
        return it->second;

    int id = static_cast<int>(_locations.size());
    _locations[name] = id;
    return id;
}

std::string Shader::loadSource(std::filesystem::path const& path)
{
    std::ifstream input(path);
    if (!input)
        throw std::runtime_error("Unable to open shader source: " + path.string());

    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    return contents;
}

} // namespace SeEditor::Graphics::OpenGL
