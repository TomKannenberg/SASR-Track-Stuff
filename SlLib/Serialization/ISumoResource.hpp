#pragma once

#include "SlLib/Resources/Database/SlResourceHeader.hpp"
#include "SlLib/Serialization/IResourceSerializable.hpp"

namespace SlLib::Serialization {

class ISumoResource : public IResourceSerializable
{
public:
    SlLib::Resources::Database::SlResourceHeader Header;
};

} // namespace SlLib::Serialization
