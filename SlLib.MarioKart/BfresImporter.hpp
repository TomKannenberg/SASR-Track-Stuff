#pragma once

#include <SlLib/Resources/Database/SlResourceDatabase.hpp>

#include <string>
#include <vector>

namespace SlLib::MarioKart {

class BfresImporter
{
public:
    explicit BfresImporter(std::string shaderCachePath);

    void RegisterModel(std::string const& path);
    void RegisterTexture(std::string const& name);
    void BuildDatabase();

    SlLib::Resources::Database::SlResourceDatabase& GetDatabase();

private:
    std::string _shaderCachePath;
    std::vector<std::string> _modelPaths;
    std::vector<std::string> _textureNames;
    SlLib::Resources::Database::SlResourceDatabase _database{SlLib::Resources::Database::SlResourceDatabase()};
};

} // namespace SlLib::MarioKart
