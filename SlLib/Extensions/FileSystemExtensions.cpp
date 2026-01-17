#include "FileSystemExtensions.hpp"

#include <span>
#include <vector>

namespace SlLib::Extensions {

bool DoesSumoToolFileExist(Filesystem::IFileSystem& fs, std::string const& path, std::string language)
{
    return fs.DoesFileExist(path + ".stz") || fs.DoesFileExist(path + "_" + language + ".stz") ||
           (fs.DoesFileExist(path + ".dat") && fs.DoesFileExist(path + ".rel")) ||
           (fs.DoesFileExist(path + "_" + language + ".dat") && fs.DoesFileExist(path + "_" + language + ".rel"));
}

SumoTool::SumoToolPackage GetSumoToolPackage(Filesystem::IFileSystem&, std::string const&,
                                            std::string)
{
    return {};
}

Resources::Database::SlResourceDatabase GetSceneDatabase(Filesystem::IFileSystem&, std::string const&,
                                                         std::string)
{
    return {};
}

bool DoesExcelDataExist(Filesystem::IFileSystem& fs, std::string const& path)
{
    return fs.DoesFileExist(path + ".dat") || fs.DoesFileExist(path + ".zat");
}

Excel::ExcelData GetExcelData(Filesystem::IFileSystem& fs, std::string const& path)
{
    std::vector<std::uint8_t> buffer;

    if (fs.DoesFileExist(path + ".zat")) {
        buffer = fs.GetFile(path + ".zat");
        Utilities::CryptUtil::DecodeBuffer(buffer);
    } else {
        buffer = fs.GetFile(path + ".dat");
    }

    return Excel::ExcelData::Load(std::span<const std::uint8_t>(buffer.data(), buffer.size()));
}

} // namespace SlLib::Extensions
