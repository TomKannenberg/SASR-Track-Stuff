#include "ContextualTimer.hpp"

#include <iomanip>
#include <iostream>

namespace SlLib::Workspace {

ContextualTimer::ContextualTimer(std::string name)
    : _name(std::move(name)),
      _start(std::chrono::steady_clock::now())
{
}

ContextualTimer::~ContextualTimer()
{
    const auto elapsed = std::chrono::steady_clock::now() - _start;
    const auto totalMilliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
    const auto minutes = totalMilliseconds / 60'000;
    const auto seconds = (totalMilliseconds / 1'000) % 60;
    const auto milliseconds = totalMilliseconds % 1'000;

    const auto fmtFlags = std::cout.flags();
    const auto fillChar = std::cout.fill('0');

    std::cout << '[' << _name << "] " << minutes << ':' << std::setw(2) << seconds << '.' << std::setw(3)
              << milliseconds << '\n';

    std::cout.fill(fillChar);
    std::cout.flags(fmtFlags);
}

} // namespace SlLib::Workspace
