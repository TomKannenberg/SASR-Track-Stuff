#include <utility>

#include "InspectorAttribute.hpp"

namespace SeEditor::Editor::Attributes {

InspectorAttribute::InspectorAttribute(std::string header)
    : Header(std::move(header))
{
}

} // namespace SeEditor::Editor::Attributes
