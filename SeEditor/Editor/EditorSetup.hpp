#pragma once

#include <string>

namespace SeEditor::Editor {

class EditorSetup
{
public:
    EditorSetup();

    std::string GamePath;
    std::string QuickstartFile;
    std::string QuickstartPlatform = "pc";
};

} // namespace SeEditor::Editor
