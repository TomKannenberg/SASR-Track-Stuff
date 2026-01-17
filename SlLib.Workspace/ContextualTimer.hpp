#pragma once

#include <chrono>
#include <string>

namespace SlLib::Workspace {

class ContextualTimer final
{
public:
    explicit ContextualTimer(std::string name);
    ContextualTimer(ContextualTimer const&) = delete;
    ContextualTimer(ContextualTimer&&) = delete;
    ContextualTimer& operator=(ContextualTimer const&) = delete;
    ContextualTimer& operator=(ContextualTimer&&) = delete;
    ~ContextualTimer();

private:
    std::string _name;
    std::chrono::steady_clock::time_point _start;
};

} // namespace SlLib::Workspace
