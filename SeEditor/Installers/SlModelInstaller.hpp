#pragma once

#include <iostream>

namespace SeEditor::Installers {

struct SlModel
{
    bool Converted = false;
};

class SlModelInstaller final
{
public:
    static void Install(SlModel& model);
};

} // namespace SeEditor::Installers
