#include "TinyFileDialog.hpp"

#include <iostream>

namespace SeEditor::Dialogs {

std::optional<std::string> TinyFileDialog::openFileDialog(FileDialogOptions const&)
{
    std::cerr << "[TinyFileDialog] openFileDialog not supported on this platform build.\n";
    return std::nullopt;
}

std::optional<std::string> TinyFileDialog::saveFileDialog(FileDialogOptions const&)
{
    std::cerr << "[TinyFileDialog] saveFileDialog not supported on this platform build.\n";
    return std::nullopt;
}

std::optional<std::string> TinyFileDialog::selectFolderDialog(std::string_view,
                                                             std::string_view)
{
    std::cerr << "[TinyFileDialog] selectFolderDialog not supported on this platform build.\n";
    return std::nullopt;
}

bool TinyFileDialog::notifyPopup(std::string_view,
                                std::string_view,
                                MessageIcon)
{
    std::cerr << "[TinyFileDialog] notifyPopup not supported on this platform build.\n";
    return false;
}

bool TinyFileDialog::messageBox(std::string_view,
                               std::string_view,
                               MessageBoxMode,
                               MessageIcon,
                               int)
{
    std::cerr << "[TinyFileDialog] messageBox not supported on this platform build.\n";
    return false;
}

std::optional<std::string> TinyFileDialog::convertResult(char const*)
{
    return std::nullopt;
}

} // namespace SeEditor::Dialogs

