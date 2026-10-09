#pragma once

// Surface：可见的窗口顶部、侧边、工作区底边。
// 坐标由窗口几何提供；角色决定如何使用表面。

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

#include "../core/math.hpp"

#include <unordered_map>
#include <vector>

namespace pets {

struct Surface {
    HWND owner{};
    int edge = 0;             // 0 top, 1 left, 2 right, 3 work-area ground
    int monitor = 0;

    V a{}, b{};
    V outward{0, -1};

    bool ground = false;

    V tangent() const { return unit(b - a); }
    double length() const { return len(b - a); }

    V at(double u) const {
        return mix(a, b, std::clamp(u, 0.0, 1.0));
    }

    double parameter(V p) const {
        V d = b - a;
        return std::clamp(dot(p - a, d) / std::max(1e-8, dot(d, d)), 0.0, 1.0);
    }

    bool sameOwner(const Surface& s) const {
        return owner == s.owner &&
               edge == s.edge &&
               monitor == s.monitor &&
               ground == s.ground;
    }
};

class Desktop {
public:
    RECT virtualRect{};
    std::vector<RECT> workAreas;
    std::vector<Surface> surfaces;

    V home{};
    HWND ownWindow{};

    void refresh(HWND own);

    const Surface* resolve(const Surface& old) const;

    const Surface* nearest(
        V p, double maximum,
        bool horizontalOnly = false,
        double minimumLength = 0) const;

    const Surface* groundAt(double x) const;
    bool onScreen(V p) const;

    // 某个窗口最近 withinSeconds 秒内是否移动/缩放过，供角色判断
    // "值得去巡检一下"的目标。
    bool recentlyMoved(HWND owner, double withinSeconds) const;

private:
    std::vector<RECT> blockers;

    std::unordered_map<HWND, RECT> previousRects;
    std::unordered_map<HWND, ULONGLONG> lastMovedTick;

    void emitVisible(
        HWND owner, int edge,
        V a, V b, V normal);

    void addWindow(HWND hwnd);
};

} // namespace pets
