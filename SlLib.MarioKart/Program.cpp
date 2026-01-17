#include "Program.hpp"
#include "TrackImportConfig.hpp"
#include "TrackImporter.hpp"

namespace SlLib::MarioKart {

int Program::Run(int /*argc*/, char** /*argv*/)
{
    TrackImportConfig config;
    config.CourseId = "Gu_Cake";
    config.TrackSource = "seasidehill2";
    config.TrackTarget = "seasidehill2";

    TrackImporter importer(config);
    importer.Import();

    return 0;
}

} // namespace SlLib::MarioKart
