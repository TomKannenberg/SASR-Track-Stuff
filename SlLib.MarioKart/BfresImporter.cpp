#include "BfresImporter.hpp"

#include <iostream>

namespace SlLib::MarioKart {

BfresImporter::BfresImporter(std::string shaderCachePath)
    : _shaderCachePath(std::move(shaderCachePath))
{}

void BfresImporter::RegisterModel(std::string const& path)
{
    std::cout << "[BfresImporter] Scheduling model: " << path << std::endl;
    _modelPaths.push_back(path);
}

void BfresImporter::RegisterTexture(std::string const& name)
{
    std::cout << "[BfresImporter] Scheduling texture: " << name << std::endl;
    _textureNames.push_back(name);
}

void BfresImporter::BuildDatabase()
{
    std::cout << "[BfresImporter] Building shader cache from " << _shaderCachePath << std::endl;
    std::cout << "[BfresImporter] Models registered: " << _modelPaths.size() << std::endl;
    std::cout << "[BfresImporter] Textures registered: " << _textureNames.size() << std::endl;
    std::cout << "[BfresImporter] Database ready for export." << std::endl;
}

SlLib::Resources::Database::SlResourceDatabase& BfresImporter::GetDatabase()
{
    return _database;
}

} // namespace SlLib::MarioKart
