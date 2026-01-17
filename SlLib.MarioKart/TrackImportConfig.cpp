#include "TrackImportConfig.hpp"

#include <utility>

namespace SlLib::MarioKart {

TrackImportConfig::TrackImportConfig(std::string courseId, std::string trackSource, std::string trackTarget)
    : CourseId(std::move(courseId))
    , TrackSource(std::move(trackSource))
    , TrackTarget(trackTarget.empty() ? "seasidehill2" : std::move(trackTarget))
{}

} // namespace SlLib::MarioKart
