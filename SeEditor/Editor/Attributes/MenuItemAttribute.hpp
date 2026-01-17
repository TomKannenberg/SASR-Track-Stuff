#pragma once

#include <string>

namespace SeEditor::Editor::Attributes {

struct MenuItemAttribute final
{
    explicit MenuItemAttribute(std::string path);

    std::string Path;
};

} // namespace SeEditor::Editor::Attributes
