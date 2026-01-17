#pragma once

namespace SlLib::SumoTool {

class ISumoToolResource
{
public:
    virtual ~ISumoToolResource() = default;
    virtual bool Load() = 0;
};

} // namespace SlLib::SumoTool
