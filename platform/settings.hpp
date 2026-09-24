#pragma once

#include "../core/math.hpp"

#include <string>

namespace pets {

class Settings {
public:
    std::wstring path;

    Settings();

    double get(
        const wchar_t* section,
        const wchar_t* key,
        double fallback) const;

    void set(
        const wchar_t* section,
        const wchar_t* key,
        double value) const;

    V position(const wchar_t* section, V fallback) const;
    void setPosition(const wchar_t* section, V p) const;
};

} // namespace pets
