#include "SlLib/IO/SlImportConfig.hpp"

#include <algorithm>
#include <filesystem>
#include <string_view>

namespace SlLib::IO {

SlImportConfig::SlImportConfig(std::shared_ptr<::SlLib::Resources::Database::SlResourceDatabase> database, std::string glbSourcePath)
    : Database(std::move(database)),
      GlbSourcePath(std::move(glbSourcePath)),
      Skeleton(nullptr)
{
    const std::filesystem::path sourcePath(GlbSourcePath);
    VirtualSceneName = sourcePath.stem().string();

    std::string directory;
    if (sourcePath.has_parent_path()) {
        directory = sourcePath.parent_path().lexically_normal().string();
    }

    std::replace(directory.begin(), directory.end(), '\\', '/');
    constexpr std::string_view prefix = "F:/";
    if (directory.rfind(prefix, 0) == 0) {
        directory.erase(0, prefix.size());
    }

    VirtualFilePath = std::move(directory);
}

bool SlImportConfig::IsCharacterImport() const
{
    return ImportType != SlImportType::Standard;
}

} // namespace SlLib::IO
