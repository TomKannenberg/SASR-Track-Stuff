#pragma once

#include "TrackImportConfig.hpp"

#include <utility>

namespace SlLib::MarioKart {

class TrackImporter
{
public:
    explicit TrackImporter(TrackImportConfig config);
    void Import();

private:
    TrackImportConfig _config;
};

} // namespace SlLib::MarioKart
