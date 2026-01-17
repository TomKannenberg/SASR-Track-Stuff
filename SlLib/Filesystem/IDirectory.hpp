#pragma once

#include <string>

namespace SlLib::Filesystem {

class IDirectory
{
public:
    virtual ~IDirectory() = default;

    virtual std::string Name() const = 0;
    virtual void SetName(std::string name) = 0;
};

} // namespace SlLib::Filesystem
