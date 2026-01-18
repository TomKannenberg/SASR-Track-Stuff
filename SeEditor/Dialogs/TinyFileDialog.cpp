#include "TinyFileDialog.hpp"

#include <string>
#include <vector>
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <cstring>
#include <iostream>

extern "C" {
static thread_local std::string g_tinyfdResult;

static const char* saveResult(std::string const& value)
{
    g_tinyfdResult = value;
    return g_tinyfdResult.c_str();
}

static std::vector<char> buildFilter(int numPatterns,
                                     const char* const* patterns,
                                     const char* description)
{
    std::vector<char> filter;
    auto append = [&filter](std::string const& text) {
        filter.insert(filter.end(), text.begin(), text.end());
        filter.push_back('\0');
    };

    if (numPatterns > 0 && patterns != nullptr && patterns[0] != nullptr)
    {
        append(description != nullptr ? description : patterns[0]);
        append(patterns[0]);
    }

    append("All Files");
    append("*.*");
    filter.push_back('\0');
    return filter;
}

static HWND pickOwnerWindow()
{
    HWND owner = GetActiveWindow();
    if (!owner)
        owner = GetForegroundWindow();
    return owner;
}

static std::wstring widenUtf8(char const* text)
{
    if (text == nullptr || *text == '\0')
        return {};
    int size = MultiByteToWideChar(CP_UTF8, 0, text, -1, nullptr, 0);
    if (size <= 0)
        return {};
    std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text, -1, result.data(), size);
    return result;
}

static std::string narrowUtf8(wchar_t const* text)
{
    if (text == nullptr || *text == L'\0')
        return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (size <= 0)
        return {};
    std::string result(static_cast<std::size_t>(size - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, result.data(), size, nullptr, nullptr);
    return result;
}

const char* tinyfd_openFileDialog(const char* aTitle,
                                  const char* aDefaultPathAndFile,
                                  int aNumOfFilterPatterns,
                                  const char* const* aFilterPatterns,
                                  const char* aSingleFilterDescription,
                                  int aAllowMultipleSelects)
{
    char fileBuffer[MAX_PATH] = {0};
    if (aDefaultPathAndFile != nullptr)
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
    if (aDefaultPathAndFile != nullptr)
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

        if (aTitle != nullptr)
        {
            std::wstring title = widenUtf8(aTitle);
            if (!title.empty())
                dialog->SetTitle(title.c_str());
        }

        if (aDefaultPathAndFile != nullptr && *aDefaultPathAndFile != '\0')
        {
            std::wstring defaultPath = widenUtf8(aDefaultPathAndFile);
            if (!defaultPath.empty())
            {
                IShellItem* folderItem = nullptr;
                if (SUCCEEDED(SHCreateItemFromParsingName(defaultPath.c_str(), nullptr,
                                                          IID_PPV_ARGS(&folderItem))))
                {
                    dialog->SetFolder(folderItem);
                    dialog->SetDefaultFolder(folderItem);
                    folderItem->Release();
                }
            }
        }

        hr = dialog->Show(pickOwnerWindow());
        if (SUCCEEDED(hr))
        {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item != nullptr)
            {
                PWSTR wpath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath != nullptr)
                {
                    std::string path = narrowUtf8(wpath);
                    CoTaskMemFree(wpath);
                    item->Release();
                    dialog->Release();
                    if (coInit && initResult != RPC_E_CHANGED_MODE)
                        CoUninitialize();
                    return path.empty() ? nullptr : saveResult(path);
                }
                item->Release();
            }
        }
        dialog->Release();
    }

    if (coInit && initResult != RPC_E_CHANGED_MODE)
        CoUninitialize();

    BROWSEINFOA bi{};
    char path[MAX_PATH] = {0};
    bi.hwndOwner = pickOwnerWindow();
    bi.lpszTitle = aTitle;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl == nullptr)
        return nullptr;

    if (!SHGetPathFromIDListA(pidl, path))
    {
        CoTaskMemFree(pidl);
        return nullptr;
    }

    CoTaskMemFree(pidl);
    return saveResult(path);
}

int tinyfd_notifyPopup(const char* aTitle, const char* aMessage, const char* aIconType)
{
    UINT flags = MB_OK | MB_ICONINFORMATION | MB_SETFOREGROUND | MB_TASKMODAL;
    if (aIconType != nullptr && std::strcmp(aIconType, "warning") == 0)
        flags = MB_OK | MB_ICONWARNING | MB_SETFOREGROUND | MB_TASKMODAL;
    else if (aIconType != nullptr && std::strcmp(aIconType, "error") == 0)
        flags = MB_OK | MB_ICONERROR | MB_SETFOREGROUND | MB_TASKMODAL;

    MessageBoxA(GetActiveWindow(), aMessage, aTitle, flags);
    return 1;
}

int tinyfd_messageBox(const char* aTitle,
                      const char* aMessage,
                      const char* aDialogType,
                      const char* aIconType,
                      int aDefaultButton)
{
    UINT flags = MB_SETFOREGROUND | MB_TASKMODAL;
    if (aDialogType != nullptr && std::strcmp(aDialogType, "yesno") == 0)
        flags |= MB_YESNO;
    else if (aDialogType != nullptr && std::strcmp(aDialogType, "okcancel") == 0)
        flags |= MB_OKCANCEL;
    else
        flags |= MB_OK;

    if (aIconType != nullptr && std::strcmp(aIconType, "warning") == 0)
        flags |= MB_ICONWARNING;
    else if (aIconType != nullptr && std::strcmp(aIconType, "error") == 0)
        flags |= MB_ICONERROR;
    else
        flags |= MB_ICONINFORMATION;

    int result = MessageBoxA(GetActiveWindow(), aMessage, aTitle, flags);
    if (flags & MB_YESNO)
        return result == IDYES;

    if (flags & MB_OKCANCEL)
        return result == IDOK;

    return result == IDOK;
}
}

namespace SeEditor::Dialogs
{

namespace
{
constexpr char ok[] = "ok";
constexpr char okcancel[] = "okcancel";
constexpr char yesno[] = "yesno";
constexpr char yesnocancel[] = "yesnocancel";

constexpr char info[] = "info";
constexpr char warning[] = "warning";
constexpr char error[] = "error";
constexpr char question[] = "question";

const char* messageBoxModeToString(MessageBoxMode mode)
{
    switch (mode)
    {
    case MessageBoxMode::Ok:
        return ok;
    case MessageBoxMode::OkCancel:
        return okcancel;
    case MessageBoxMode::YesNo:
        return yesno;
    case MessageBoxMode::YesNoCancel:
        return yesnocancel;
    }

    return ok;
}

const char* messageIconToString(MessageIcon icon)
{
    switch (icon)
    {
    case MessageIcon::Info:
        return info;
    case MessageIcon::Warning:
        return warning;
    case MessageIcon::Error:
        return error;
    case MessageIcon::Question:
        return question;
    }

    return info;
}

std::vector<char const*> collectFilterPatterns(std::vector<std::string> const& patterns)
{
    std::vector<char const*> result;
    result.reserve(patterns.size());
    for (auto const& pattern : patterns)
        result.push_back(pattern.c_str());
    return result;
}

} // namespace

std::optional<std::string> TinyFileDialog::openFileDialog(FileDialogOptions const& options)
{
    auto patternStorage = collectFilterPatterns(options.FilterPatterns);
    char const* description = options.FilterDescription.has_value() ? options.FilterDescription->c_str() : nullptr;
    char const* defaultPath = options.DefaultPathAndFile.empty() ? nullptr : options.DefaultPathAndFile.c_str();

    char const* result = tinyfd_openFileDialog(
        options.Title.c_str(),
        defaultPath,
        static_cast<int>(patternStorage.size()),
        patternStorage.empty() ? nullptr : patternStorage.data(),
        description,
        options.AllowMultipleSelects ? 1 : 0);

    return convertResult(result);
}

std::optional<std::string> TinyFileDialog::saveFileDialog(FileDialogOptions const& options)
{
    auto patternStorage = collectFilterPatterns(options.FilterPatterns);
    char const* description = options.FilterDescription.has_value() ? options.FilterDescription->c_str() : nullptr;
    char const* defaultPath = options.DefaultPathAndFile.empty() ? nullptr : options.DefaultPathAndFile.c_str();

    char const* result = tinyfd_saveFileDialog(
        options.Title.c_str(),
        defaultPath,
        static_cast<int>(patternStorage.size()),
        patternStorage.empty() ? nullptr : patternStorage.data(),
        description);

    return convertResult(result);
}

std::optional<std::string> TinyFileDialog::selectFolderDialog(std::string_view title,
                                                              std::string_view defaultPath)
{
    std::string titleBuffer(title);
    std::string defaultPathBuffer(defaultPath);
    char const* defaultPathCStr = defaultPathBuffer.empty() ? nullptr : defaultPathBuffer.c_str();

    char const* result = tinyfd_selectFolderDialog(titleBuffer.c_str(), defaultPathCStr);
    return convertResult(result);
}

bool TinyFileDialog::notifyPopup(std::string_view title,
                                std::string_view message,
                                MessageIcon icon)
{
    std::string titleBuffer(title);
    std::string messageBuffer(message);
    return tinyfd_notifyPopup(titleBuffer.c_str(), messageBuffer.c_str(), messageIconToString(icon)) != 0;
}

bool TinyFileDialog::messageBox(std::string_view title,
                               std::string_view message,
                               MessageBoxMode mode,
                               MessageIcon icon,
                               int defaultButton)
{
    std::string titleBuffer(title);
    std::string messageBuffer(message);
    return tinyfd_messageBox(titleBuffer.c_str(),
                             messageBuffer.c_str(),
                             messageBoxModeToString(mode),
                             messageIconToString(icon),
                             defaultButton) != 0;
}

std::optional<std::string> TinyFileDialog::convertResult(char const* result)
{
    if (result == nullptr)
        return std::nullopt;

    return std::string(result);
}

} // namespace SeEditor::Dialogs
