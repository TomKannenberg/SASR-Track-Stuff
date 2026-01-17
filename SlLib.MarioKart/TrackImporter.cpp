#include "TrackImporter.hpp"

#include "BfresImporter.hpp"
#include "CollisionImporter.hpp"
#include "TrackImportConfig.hpp"

#include <iostream>

namespace SlLib::MarioKart {

TrackImporter::TrackImporter(TrackImportConfig config)
    : _config(std::move(config))
{}

void TrackImporter::Import()
{
    std::cout << "[TrackImporter] Course: " << _config.CourseId << std::endl;
    std::cout << "[TrackImporter] Source: " << _config.TrackSource << ", target: " << _config.TrackTarget << std::endl;

    BfresImporter importer(R"(F:\sart\cache\shadercache)");
    importer.RegisterModel("gwii_moomoomeadows");
    importer.RegisterTexture("se_texture");
    importer.BuildDatabase();

    CollisionImporter collision(_config.CourseId);
    collision.SetScale(0.1f);
    collision.ImportCollision("collision_main");

    auto& database = importer.GetDatabase();
    std::cout << "[TrackImporter] Database chunk count: " << database.Chunks.size() << std::endl;
}

} // namespace SlLib::MarioKart
