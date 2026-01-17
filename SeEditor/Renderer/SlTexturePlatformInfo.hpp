#pragma once

#include <array>
#include <string>

namespace SeEditor::Renderer {

class SlTexturePlatformInfo final
{
public:
    struct Info
    {
        std::string Name;
        bool Valid;
        bool Compressed;
        int Stride;

        bool IsValid() const noexcept { return Valid; }
        bool IsCompressedType() const noexcept { return Compressed; }
    };

    static Info const& Lookup(int index);

private:
    static inline Info const DefaultInfo{"Unknown", false, false, 0};
    static inline std::array<Info, 3> const s_info{
        Info{"RGBA8", true, false, 32},
        Info{"BC1", true, true, 8},
        Info{"BC7", true, true, 16},
    };
};

} // namespace SeEditor::Renderer
