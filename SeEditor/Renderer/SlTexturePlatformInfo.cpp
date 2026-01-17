#include "SlTexturePlatformInfo.hpp"

namespace SeEditor::Renderer {

SlTexturePlatformInfo::Info const& SlTexturePlatformInfo::Lookup(int index)
{
    if (index < 0 || index >= static_cast<int>(s_info.size()))
        return DefaultInfo;

    return s_info[static_cast<std::size_t>(index)];
}

} // namespace SeEditor::Renderer
