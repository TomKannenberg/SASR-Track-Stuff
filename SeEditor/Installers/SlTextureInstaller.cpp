#include "SlTextureInstaller.hpp"

namespace SeEditor::Installers {

void SlTextureInstaller::Install(SlTexture& texture)
{
    if (texture.ID != 0 || !texture.HasData())
    {
        std::cout << "SlTexture installation skipped.\n";
        return;
    }

    texture.ID = 1;
    std::cout << "SlTexture installation stub executed.\n";
}

} // namespace SeEditor::Installers
