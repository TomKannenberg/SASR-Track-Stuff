#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <SlLib/Excel/ExcelData.hpp>

namespace SlLib::Resources::Database {
class SlResourceDatabase;
}

class SlFile final
{
public:
    static void AddGameDataFolder(std::string const& root);
    static bool DoesFileExist(std::string const& path);
    static std::optional<std::vector<std::uint8_t>> GetFile(std::string const& path);
    static std::optional<SlLib::Excel::ExcelData> GetExcelData(std::string const& path);
    static std::optional<SlLib::Resources::Database::SlResourceDatabase> GetSceneDatabase(std::string const& path,
                                                                                           std::string const& extension = "pc");

private:
    static std::optional<std::filesystem::path> findFile(std::string const& path);

    static std::vector<std::filesystem::path> s_gameDataFolders;
};
