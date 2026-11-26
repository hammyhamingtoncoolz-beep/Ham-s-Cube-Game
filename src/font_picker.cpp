#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "font_picker.h"

#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>

namespace {
std::string toUtf8(const wchar_t* value) {
    const int length = WideCharToMultiByte(CP_UTF8, 0, value, -1, nullptr, 0, nullptr, nullptr);
    if (length <= 1) return {};
    std::string result(static_cast<size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value, -1, result.data(), length, nullptr, nullptr);
    result.resize(static_cast<size_t>(length - 1));
    return result;
}
}

std::string ChooseFontFile() {
    wchar_t path[MAX_PATH]{};
    OPENFILENAMEW dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = GetActiveWindow();
    dialog.lpstrFile = path;
    dialog.nMaxFile = MAX_PATH;
    dialog.lpstrFilter = L"Font files (*.ttf;*.otf)\0*.ttf;*.otf\0TrueType fonts (*.ttf)\0*.ttf\0OpenType fonts (*.otf)\0*.otf\0All files (*.*)\0*.*\0\0";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    return GetOpenFileNameW(&dialog) ? toUtf8(path) : "";
}
#else
std::string ChooseFontFile() { return {}; }
#endif

