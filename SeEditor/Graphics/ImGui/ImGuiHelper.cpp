#include "ImGuiHelper.hpp"
#include "ImGuiFontList.hpp"

#include <imgui.h>
#include <array>
#include <cstring>

namespace SeEditor::Graphics::ImGui {

std::string DoLabelPrefix(std::string_view label)
{
    float width = ::ImGui::CalcItemWidth();
    float x = ::ImGui::GetCursorPosX();

    ::ImGui::TextUnformatted(label.data(), label.data() + label.size());
    ::ImGui::SameLine();
    ::ImGui::SetCursorPosX(x + width * 0.5f + ::ImGui::GetStyle().ItemInnerSpacing.x);
    ::ImGui::SetNextItemWidth(-1.0f);

    return std::string("##") + std::string(label);
}

void DoBoldText(std::string_view text)
{
    ImGuiIO& io = ::ImGui::GetIO();
    int const boldIndex = static_cast<int>(ImGuiFontList::InterBold);
    ImFont* boldFont = io.Fonts->Fonts.size() > static_cast<size_t>(boldIndex) ? io.Fonts->Fonts[boldIndex] : nullptr;
    if (boldFont != nullptr)
        ::ImGui::PushFont(boldFont);

    ::ImGui::TextUnformatted(text.data(), text.data() + text.size());

    if (boldFont != nullptr)
        ::ImGui::PopFont();
}

bool DoIndexedEnum(std::string_view label, int& value)
{
    return ::ImGui::InputInt(label.data(), &value);
}

bool StartPropertyTable(std::string_view name)
{
    if (!::ImGui::CollapsingHeader(name.data(), ImGuiTreeNodeFlags_DefaultOpen))
        return false;

    ::ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{2.0f, 2.0f});
    std::string tableId = std::string("##propertytable_") + std::string(name);
    return ::ImGui::BeginTable(tableId.c_str(), 2, ImGuiTableFlags_PadOuterX);
}

void StartNewLine()
{
    ::ImGui::TableNextRow();
    ::ImGui::TableNextColumn();
}

void DoLabel(std::string_view text)
{
    ::ImGui::AlignTextToFramePadding();
    ::ImGui::TextUnformatted(text.data(), text.data() + text.size());
    ::ImGui::TableNextColumn();
}

void DrawHeader(std::string_view text)
{
    StartNewLine();
    DoBoldText(text);
}

bool DrawCheckbox(std::string_view text, bool& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);
    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::Checkbox("##value", &value);
    ::ImGui::PopID();
    return changed;
}

void DrawCheckboxFlags(std::string_view text, int& value, int flag)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);
    ::ImGui::SetNextItemWidth(-1.0f);
    ::ImGui::CheckboxFlags("##value", &value, flag);
    ::ImGui::PopID();
}

bool DrawInputText(std::string_view text, std::string& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);
    ::ImGui::SetNextItemWidth(-1.0f);

    char buffer[256];
    std::strncpy(buffer, value.c_str(), sizeof(buffer));
    buffer[sizeof(buffer) - 1] = 0;

    bool changed = ::ImGui::InputText("##value", buffer, sizeof(buffer));
    if (changed)
        value = buffer;

    ::ImGui::PopID();
    return changed;
}

bool DrawInputInt(std::string_view text, int& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::InputInt("##value", &value);

    ::ImGui::PopID();
    return changed;
}

bool DrawDragFloat(std::string_view text, float& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::DragFloat("##value", &value);

    ::ImGui::PopID();
    return changed;
}

bool DrawColor3(std::string_view text, SlLib::Math::Vector3& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    std::array<float, 3> buffer = {value.X, value.Y, value.Z};
    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::ColorEdit3("##value", buffer.data());
    if (changed)
    {
        value.X = buffer[0];
        value.Y = buffer[1];
        value.Z = buffer[2];
    }

    ::ImGui::PopID();
    return changed;
}

bool DrawColor4(std::string_view text, SlLib::Math::Vector4& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    std::array<float, 4> buffer = {value.X, value.Y, value.Z, value.W};
    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::ColorEdit4("##value", buffer.data());
    if (changed)
    {
        value.X = buffer[0];
        value.Y = buffer[1];
        value.Z = buffer[2];
        value.W = buffer[3];
    }

    ::ImGui::PopID();
    return changed;
}

bool DrawDragFloat3(std::string_view text, SlLib::Math::Vector3& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    std::array<float, 3> buffer = {value.X, value.Y, value.Z};
    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::DragFloat3("##value", buffer.data());
    if (changed)
    {
        value.X = buffer[0];
        value.Y = buffer[1];
        value.Z = buffer[2];
    }

    ::ImGui::PopID();
    return changed;
}

bool DrawDragFloat4(std::string_view text, SlLib::Math::Vector4& value)
{
    StartNewLine();
    ::ImGui::PushID(text.data());
    DoLabel(text);

    std::array<float, 4> buffer = {value.X, value.Y, value.Z, value.W};
    ::ImGui::SetNextItemWidth(-1.0f);
    bool changed = ::ImGui::DragFloat4("##value", buffer.data());
    if (changed)
    {
        value.X = buffer[0];
        value.Y = buffer[1];
        value.Z = buffer[2];
        value.W = buffer[3];
    }

    ::ImGui::PopID();
    return changed;
}

void EndPropertyTable()
{
    ::ImGui::EndTable();
    ::ImGui::PopStyleVar();
}

} // namespace SeEditor::Graphics::ImGui
