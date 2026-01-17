#pragma once

#include <iostream>

namespace SeEditor::Installers {

struct SlTexture
{
    int ID = 0;
    int DataSize = 0;

    bool HasData() const noexcept
    {
        return DataSize > 0;
    }
};

class SlTextureInstaller final
{
public:
    static void Install(SlTexture& texture);
};

} // namespace SeEditor::Installers
