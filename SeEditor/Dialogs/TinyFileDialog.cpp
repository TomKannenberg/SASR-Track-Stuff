#include "TinyFileDialog.hpp"

#include <string>
#include <vector>
#include <iostream>
#include <cstring>

#if defined(_WIN32)
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>

namespace {

thread_local std::string g_tinyfdResult;

const char* saveResult(std::string const& value)
{
    g_tinyfdResult = value;
    return g_tinyfdResult.c_str();
}

std::vector<char> buildFilter(int numPatterns,
                              const char* const* patterns,
                              const char* description)
{
    std::vector<char> filter;
    auto append = [&filter](std::string const& text) {
        filter.insert(filter.end(), text.begin(), text.end());
        filter.push_back('\0');
    };

    if (numPatterns > 0 && patterns && patterns[0])
    {
        append(description ? description : patterns[0]);
        append(patterns[0]);
    }

    append("All Files");
    append("*.*");
    filter.push_back('\0');
    return filter;
}

HWND pickOwnerWindow()
{
    HWND owner = GetActiveWindow();
    if (!owner)
        owner = GetForegroundWindow();
    return owner;
}

std::wstring widenUtf8(char const* text)
{
    if (!text || *text == '\0')
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
    return result;
}

std::string narrowUtf8(wchar_t const* text)
{
    if (!text || *text == L'\0')
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

} // namespace

extern "C" {

const char* tinyfd_openFileDialog(const char* aTitle,
                                  const char* aDefaultPathAndFile,
                                  int aNumOfFilterPatterns,
                                  const char* const* aFilterPatterns,
                                  const char* aSingleFilterDescription,
                                  int aAllowMultipleSelects)
{
    char fileBuffer[MAX_PATH] = {0};
    if (aDefaultPathAndFile)
        std::strncpy(fileBuffer, aDefaultPathAndFile, MAX_PATH - 1);

    auto filter = buildFilter(aNumOfFilterPatterns, aFilterPatterns, aSingleFilterDescription);

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = pickOwnerWindow();
    ofn.lpstrTitle = aTitle;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter.empty() ? nullptr : filter.data();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    if (aAllowMultipleSelects)
        ofn.Flags |= OFN_ALLOWMULTISELECT;

    if (GetOpenFileNameA(&ofn))
        return saveResult(fileBuffer);

    DWORD err = CommDlgExtendedError();
    if (err != 0)
        std::cerr << "[TinyFileDialog] openFileDialog failed err=" << err << "\n";
    return nullptr;
}

const char* tinyfd_saveFileDialog(const char* aTitle,
                                  const char* aDefaultPathAndFile,
                                  int aNumOfFilterPatterns,
                                  const char* const* aFilterPatterns,
                                  const char* aSingleFilterDescription)
{
    char fileBuffer[MAX_PATH] = {0};
    if (aDefaultPathAndFile)
        std::strncpy(fileBuffer, aDefaultPathAndFile, MAX_PATH - 1);

    auto filter = buildFilter(aNumOfFilterPatterns, aFilterPatterns, aSingleFilterDescription);

    OPENFILENAMEA ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = pickOwnerWindow();
    ofn.lpstrTitle = aTitle;
    ofn.lpstrFile = fileBuffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = filter.empty() ? nullptr : filter.data();
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (GetSaveFileNameA(&ofn))
        return saveResult(fileBuffer);

    DWORD err = CommDlgExtendedError();
    if (err != 0)
        std::cerr << "[TinyFileDialog] saveFileDialog failed err=" << err << "\n";
    return nullptr;
}

const char* tinyfd_selectFolderDialog(const char* aTitle, const char* aDefaultPathAndFile)
{
    HRESULT initResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool coInit = SUCCEEDED(initResult) || initResult == RPC_E_CHANGED_MODE;

    IFileDialog* dialog = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&dialog));
    if (SUCCEEDED(hr))
    {
        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);

        if (aTitle)
            dialog->SetTitle(widenUtf8(aTitle).c_str());
        if (aDefaultPathAndFile)
        {
            IShellItem* folder = nullptr;
            if (SUCCEEDED(SHCreateItemFromParsingName(widenUtf8(aDefaultPathAndFile).c_str(), nullptr, IID_PPV_ARGS(&folder))))
            {
                dialog->SetFolder(folder);
                folder->Release();
            }
        }

        hr = dialog->Show(pickOwnerWindow());
        if (SUCCEEDED(hr))
        {
            IShellItem* result = nullptr;
            if (SUCCEEDED(dialog->GetResult(&result)))
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(result->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                {
                    auto narrowed = narrowUtf8(path);
                    CoTaskMemFree(path);
                    result->Release();
                    dialog->Release();
                    if (coInit)
                        CoUninitialize();
                    return saveResult(narrowed);
                }
                if (path)
                    CoTaskMemFree(path);
                result->Release();
            }
        }
        dialog->Release();
    }

    if (coInit)
        CoUninitialize();
    return nullptr;
}

} // extern "C"

namespace SeEditor::Dialogs {

std::optional<std::string> TinyFileDialog::convertResult(char const* result)
{
    if (result == nullptr || *result == '\0')
        return std::nullopt;
    return std::string(result);
}

std::optional<std::string> TinyFileDialog::openFileDialog(FileDialogOptions const& options)
{
    return convertResult(tinyfd_openFileDialog(
        options.Title.c_str(),
        options.DefaultPathAndFile.empty() ? nullptr : options.DefaultPathAndFile.c_str(),
        static_cast<int>(options.FilterPatterns.size()),
        options.FilterPatterns.empty()
            ? nullptr
            : [&]() {
                  static std::vector<const char*> ptrs;
                  ptrs.clear();
                  for (auto const& s : options.FilterPatterns) ptrs.push_back(s.c_str());
                  return ptrs.data();
              }(),
        options.FilterDescription ? options.FilterDescription->c_str() : nullptr,
        options.AllowMultipleSelects ? 1 : 0));
}

std::optional<std::string> TinyFileDialog::saveFileDialog(FileDialogOptions const& options)
{
    return convertResult(tinyfd_saveFileDialog(
        options.Title.c_str(),
        options.DefaultPathAndFile.empty() ? nullptr : options.DefaultPathAndFile.c_str(),
        static_cast<int>(options.FilterPatterns.size()),
        options.FilterPatterns.empty()
            ? nullptr
            : [&]() {
                  static std::vector<const char*> ptrs;
                  ptrs.clear();
                  for (auto const& s : options.FilterPatterns) ptrs.push_back(s.c_str());
                  return ptrs.data();
              }(),
        options.FilterDescription ? options.FilterDescription->c_str() : nullptr));
}

std::optional<std::string> TinyFileDialog::selectFolderDialog(std::string_view title,
                                                             std::string_view defaultPath)
{
    return convertResult(tinyfd_selectFolderDialog(
        title.empty() ? nullptr : std::string(title).c_str(),
        defaultPath.empty() ? nullptr : std::string(defaultPath).c_str()));
}

bool TinyFileDialog::notifyPopup(std::string_view title,
                                std::string_view message,
                                MessageIcon icon)
{
    UINT flags = MB_OK;
    switch (icon)
    {
    case MessageIcon::Warning: flags |= MB_ICONWARNING; break;
    case MessageIcon::Error: flags |= MB_ICONERROR; break;
    case MessageIcon::Question: flags |= MB_ICONQUESTION; break;
    default: flags |= MB_ICONINFORMATION; break;
    }
    int res = MessageBoxA(pickOwnerWindow(),
                          std::string(message).c_str(),
                          std::string(title).c_str(),
                          flags);
    return res != 0;
}

bool TinyFileDialog::messageBox(std::string_view title,
                               std::string_view message,
                               MessageBoxMode mode,
                               MessageIcon icon,
                               int defaultButton)
{
    UINT flags = 0;
    switch (mode)
    {
    case MessageBoxMode::Ok: flags = MB_OK; break;
    case MessageBoxMode::OkCancel: flags = MB_OKCANCEL; break;
    case MessageBoxMode::YesNo: flags = MB_YESNO; break;
    case MessageBoxMode::YesNoCancel: flags = MB_YESNOCANCEL; break;
    }
    switch (icon)
    {
    case MessageIcon::Warning: flags |= MB_ICONWARNING; break;
    case MessageIcon::Error: flags |= MB_ICONERROR; break;
    case MessageIcon::Question: flags |= MB_ICONQUESTION; break;
    default: flags |= MB_ICONINFORMATION; break;
    }
    if (defaultButton == 2)
        flags |= MB_DEFBUTTON2;
    else if (defaultButton == 3)
        flags |= MB_DEFBUTTON3;
    int res = MessageBoxA(pickOwnerWindow(),
                          std::string(message).c_str(),
                          std::string(title).c_str(),
                          flags);
    return res == IDOK || res == IDYES;
}

} // namespace SeEditor::Dialogs

#else // !_WIN32

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

#endif
