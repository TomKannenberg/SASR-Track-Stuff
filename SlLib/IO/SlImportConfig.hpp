#pragma once

#include "SlImportType.hpp"

#include "SlLib/Resources/Database/SlResourceDatabase.hpp"
#include "SlLib/Resources/SlSkeleton.hpp"

#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace SlLib::IO {

class SlImportConfig
{
public:
    using BoneRemapList = std::vector<std::pair<std::string, int>>;

    SlImportConfig(std::shared_ptr<::SlLib::Resources::Database::SlResourceDatabase> database, std::string glbSourcePath);

    [[nodiscard]] bool IsCharacterImport() const;

    std::shared_ptr<::SlLib::Resources::Database::SlResourceDatabase> Database;
    std::string GlbSourcePath;
    std::shared_ptr<::SlLib::Resources::SlSkeleton> Skeleton;
    std::string VirtualSceneName;
    std::string VirtualFilePath;
    SlImportType ImportType = SlImportType::Standard;
    std::function<std::string(BoneRemapList&, int)> BoneRemapCallback;
};

} // namespace SlLib::IO
