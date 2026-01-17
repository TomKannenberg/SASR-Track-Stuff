#include "SlCollisionIO.hpp"

#include "SlLib/Resources/SlResourceCollision.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace SlLib::IO {

void SlCollisionIO::Export(Resources::SlResourceCollision const&, std::filesystem::path path)
{
    if (auto dir = path.parent_path(); !dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Failed to create collision export file: " + path.string());
    }

    output << "# Collision export placeholder\n";
}

} // namespace SlLib::IO
