#pragma once

#include "SlRelocationType.hpp"

namespace SlLib::Resources::Database {

struct SlResourceRelocation
{
    int Offset = 0;
    int Value = 0;

    SlRelocationType RelocationType() const
    {
        return static_cast<SlRelocationType>(Value & 0x3);
    }

    int ResourceType() const
    {
        return Value >> 4;
    }

    bool IsGpuPointer() const
    {
        return RelocationType() == SlRelocationType::Pointer && ResourceType() != 0;
    }

    bool IsResourcePointer() const
    {
        SlRelocationType type = RelocationType();
        return type == SlRelocationType::Resource || type == SlRelocationType::ResourcePair;
    }
};

} // namespace SlLib::Resources::Database
