#pragma once

#include <string>

namespace SlLib::Resources::Database {

class SlPlatform
{
public:
    SlPlatform() = default;
    SlPlatform(std::string extension, bool isBigEndian, bool is64Bit, int defaultVersion);

    [[nodiscard]] bool IsBigEndian() const { return m_isBigEndian; }
    [[nodiscard]] bool Is64Bit() const { return m_is64Bit; }
    [[nodiscard]] int DefaultVersion() const { return m_defaultVersion; }

private:
    std::string m_extension;
    bool m_isBigEndian = false;
    bool m_is64Bit = false;
    int m_defaultVersion = 0;
};

} // namespace SlLib::Resources::Database
