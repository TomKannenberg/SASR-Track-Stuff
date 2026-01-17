#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace SeEditor::Dialogs {

enum class MessageBoxMode
{
    Ok,
    OkCancel,
    YesNo,
    YesNoCancel,
};

enum class MessageIcon
{
    Info,
    Warning,
    Error,
    Question,
};

struct FileDialogOptions
{
    std::string Title;
    std::string DefaultPathAndFile;
    std::vector<std::string> FilterPatterns;
    std::optional<std::string> FilterDescription;
    bool AllowMultipleSelects = false;
};

class TinyFileDialog final
{
public:
    static std::optional<std::string> openFileDialog(FileDialogOptions const& options);
    static std::optional<std::string> saveFileDialog(FileDialogOptions const& options);
    static std::optional<std::string> selectFolderDialog(std::string_view title,
                                                        std::string_view defaultPath = {});
    static bool notifyPopup(std::string_view title,
                            std::string_view message,
                            MessageIcon icon = MessageIcon::Info);
    static bool messageBox(std::string_view title,
                           std::string_view message,
                           MessageBoxMode mode = MessageBoxMode::Ok,
                           MessageIcon icon = MessageIcon::Info,
                           int defaultButton = 1);

private:
    static std::optional<std::string> convertResult(char const* result);
};

} // namespace SeEditor::Dialogs
