#include "SlModelInstaller.hpp"

namespace SeEditor::Installers {

void SlModelInstaller::Install(SlModel& model)
{
    if (model.Converted)
    {
        std::cout << "SlModel already converted.\n";
        return;
    }

    model.Converted = true;
    std::cout << "SlModel conversion stub executed.\n";
}

} // namespace SeEditor::Installers
