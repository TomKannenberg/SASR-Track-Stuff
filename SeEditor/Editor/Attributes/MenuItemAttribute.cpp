#include <utility>

#include "MenuItemAttribute.hpp"

namespace SeEditor::Editor::Attributes {

MenuItemAttribute::MenuItemAttribute(std::string path)
    : Path(std::move(path))
{
}

} // namespace SeEditor::Editor::Attributes
