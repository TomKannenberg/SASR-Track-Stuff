#pragma once

#include "../Excel/ExcelData.hpp"
#include "../Filesystem/IFileSystem.hpp"
#include "../Utilities/CryptUtil.hpp"

#include <string>

namespace SlLib::SumoTool {
struct SumoToolPackage
{
    SumoToolPackage() = default;
};
}

namespace SlLib::Resources::Database {
struct SlResourceDatabase
{
    SlResourceDatabase() = default;
};
}

namespace SlLib::Extensions {

bool DoesSumoToolFileExist(Filesystem::IFileSystem& fs, std::string const& path, std::string language = "en");

SumoTool::SumoToolPackage GetSumoToolPackage(Filesystem::IFileSystem& fs, std::string const& path,
                                            std::string language = "en");

Resources::Database::SlResourceDatabase GetSceneDatabase(Filesystem::IFileSystem& fs, std::string const& path,
                                                         std::string extension = "pc");

bool DoesExcelDataExist(Filesystem::IFileSystem& fs, std::string const& path);

Excel::ExcelData GetExcelData(Filesystem::IFileSystem& fs, std::string const& path);

} // namespace SlLib::Extensions
