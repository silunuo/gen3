#include "settings.hpp"

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shlobj.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")

namespace pets {

Settings::Settings() {
    PWSTR directory = nullptr;

    if (SUCCEEDED(SHGetKnownFolderPath(
            FOLDERID_LocalAppData, 0, nullptr, &directory)))
    {
        path = directory;
        CoTaskMemFree(directory);
    } else {
        path = L".";
    }

    path += L"\\DesktopPets";
    CreateDirectoryW(path.c_str(), nullptr);
    path += L"\\state.ini";
}

double Settings::get(
    const wchar_t* section,
    const wchar_t* key,
    double fallback) const
{
    wchar_t value[128]{};
    std::wstring def = std::to_wstring(fallback);

    GetPrivateProfileStringW(
        section, key, def.c_str(),
        value, 128, path.c_str());

    return std::wcstod(value, nullptr);
}

void Settings::set(
    const wchar_t* section,
    const wchar_t* key,
    double value) const
{
    std::wstring text = std::to_wstring(value);

    WritePrivateProfileStringW(
        section, key, text.c_str(), path.c_str());
}

V Settings::position(const wchar_t* section, V fallback) const {
    return {
        get(section, L"x", fallback.x),
        get(section, L"y", fallback.y)
    };
}

void Settings::setPosition(const wchar_t* section, V p) const {
    set(section, L"x", p.x);
    set(section, L"y", p.y);
}

} // namespace pets
