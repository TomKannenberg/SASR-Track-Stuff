#pragma once

namespace SlLib::Resources::Database {
class SlPlatform;

class SlPlatformContext
{
public:
    SlPlatform* Platform = nullptr;
    int Version = 0;
    bool IsSSR = false;
};
} // namespace SlLib::Resources::Database
