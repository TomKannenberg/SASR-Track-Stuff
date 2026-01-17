#pragma once

#include <string>

namespace SlLib::Serialization {

class Crumb
{
public:
    Crumb();
    ~Crumb();

    std::string Message;
};

} // namespace SlLib::Serialization
