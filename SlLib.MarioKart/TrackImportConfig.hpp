#pragma once

#include <string>

namespace SlLib::MarioKart {

struct TrackImportConfig
{
    std::string CourseId;
    std::string TrackSource;
    std::string TrackTarget = "seasidehill2";

    TrackImportConfig() = default;
    TrackImportConfig(std::string courseId, std::string trackSource, std::string trackTarget = "seasidehill2");
};

} // namespace SlLib::MarioKart
