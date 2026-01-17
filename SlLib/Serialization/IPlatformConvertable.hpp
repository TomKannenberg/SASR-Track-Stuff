#pragma once

namespace SlLib::Serialization {

class IPlatformConvertable
{
public:
    virtual ~IPlatformConvertable() = default;
    virtual void ConvertPlatform() = 0;
};

} // namespace SlLib::Serialization
