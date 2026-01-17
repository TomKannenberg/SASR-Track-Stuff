#pragma once

#include <imgui.h>
#include <string>
#include <string_view>

#include "../../../SlLib/Math/Vector.hpp"

namespace SeEditor::Graphics::ImGui {

std::string DoLabelPrefix(std::string_view label);

void DoBoldText(std::string_view text);

bool DoIndexedEnum(std::string_view label, int& value);

bool StartPropertyTable(std::string_view name);

void StartNewLine();

void DoLabel(std::string_view text);

void DrawHeader(std::string_view text);

bool DrawCheckbox(std::string_view text, bool& value);

void DrawCheckboxFlags(std::string_view text, int& value, int flag);

bool DrawInputText(std::string_view text, std::string& value);

bool DrawInputInt(std::string_view text, int& value);

bool DrawDragFloat(std::string_view text, float& value);

bool DrawColor3(std::string_view text, SlLib::Math::Vector3& value);

bool DrawColor4(std::string_view text, SlLib::Math::Vector4& value);

bool DrawDragFloat3(std::string_view text, SlLib::Math::Vector3& value);

bool DrawDragFloat4(std::string_view text, SlLib::Math::Vector4& value);

void EndPropertyTable();

template <typename Enum>
bool DrawIndexedEnum(std::string_view text, Enum& value)
{
    int stored = static_cast<int>(value);
    bool changed = DoIndexedEnum(text, stored);
    if (changed)
        value = static_cast<Enum>(stored);

    return changed;
}

template <typename Enum>
void DoHashedEnum(std::string_view label, Enum& value)
{
    int stored = static_cast<int>(value);
    if (::ImGui::InputInt(label.data(), &stored))
        value = static_cast<Enum>(stored);
}

} // namespace SeEditor::Graphics::ImGui
