#pragma once

#include <filesystem>
#include <string>

namespace SlLib::Resources {
class SlResourceCollision;
}

namespace SlLib::IO {

class SlCollisionIO
{
public:
    static void Export(Resources::SlResourceCollision const& collision, std::filesystem::path path);
};

} // namespace SlLib::IO
