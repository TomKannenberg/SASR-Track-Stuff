#pragma once

#include "SlLib/Serialization/ISumoResource.hpp"

namespace SlLib::Resources::Database {

class IResourceTypeHandler
{
public:
    virtual ~IResourceTypeHandler() = default;
    virtual void Install(Serialization::ISumoResource* resource) = 0;
    virtual void Uninstall(Serialization::ISumoResource* resource) = 0;
};

} // namespace SlLib::Resources::Database
