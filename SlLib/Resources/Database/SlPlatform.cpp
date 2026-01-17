#include "SlPlatform.hpp"

#include <utility>

namespace SlLib::Resources::Database {

SlPlatform::SlPlatform(std::string extension, bool isBigEndian, bool is64Bit, int defaultVersion)
    : m_extension(std::move(extension)),
      m_isBigEndian(isBigEndian),
      m_is64Bit(is64Bit),
      m_defaultVersion(defaultVersion)
{
}

} // namespace SlLib::Resources::Database
