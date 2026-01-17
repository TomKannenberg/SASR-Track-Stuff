#pragma once

#include <string>

namespace SeEditor::Editor::Attributes {

class InspectorAttribute final
{
public:
    explicit InspectorAttribute(std::string header);

    std::string Header;
};

} // namespace SeEditor::Editor::Attributes
