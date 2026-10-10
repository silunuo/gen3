#include "environment.hpp"

#include <dwmapi.h>

#include <algorithm>
#include <cstdlib>
#include <utility>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "dwmapi.lib")

namespace pets {

void Desktop::refresh(HWND own) {
    ownWindow = own;

    virtualRect.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    virtualRect.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    virtualRect.right =
        virtualRect.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    virtualRect.bottom =
        virtualRect.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    workAreas.clear();
    surfaces.clear();
    blockers.clear();
    frameWindows.clear();

    EnumDisplayMonitors(
        nullptr, nullptr,
        [](HMONITOR monitor, HDC, LPRECT, LPARAM context) -> BOOL {
            auto& self = *reinterpret_cast<Desktop*>(context);

            MONITORINFO info{};
            info.cbSize = sizeof(info);
            GetMonitorInfoW(monitor, &info);

            if (info.dwFlags & MONITORINFOF_PRIMARY) {
                self.home = {
                    (info.rcWork.left + info.rcWork.right) * 0.5,
                    (info.rcWork.top + info.rcWork.bottom) * 0.5
                };
            }

            self.workAreas.push_back(info.rcWork);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(this));

    EnumWindows(
        [](HWND hwnd, LPARAM context) -> BOOL {
            reinterpret_cast<Desktop*>(context)->addWindow(hwnd);
            return TRUE;
        },
        reinterpret_cast<LPARAM>(this));

    for (size_t i = 0; i < workAreas.size(); ++i) {
        const RECT& r = workAreas[i];

        surfaces.push_back({
            nullptr, 3, static_cast<int>(i),
            {static_cast<double>(r.left), static_cast<double>(r.bottom)},
            {static_cast<double>(r.right), static_cast<double>(r.bottom)},
            {0, -1},
            true
        });
    }

    ULONGLONG now = GetTickCount64();

    for (HWND hwnd : frameWindows) {
        bool existedBefore = std::find(
            previousFrameWindows.begin(), previousFrameWindows.end(),
            hwnd) != previousFrameWindows.end();

        if (!existedBefore) {
            auto it = previousRects.find(hwnd);

            if (it != previousRects.end()) {
                const RECT& r = it->second;

                opens.push_back({
                    {(r.left + r.right) * 0.5, (r.top + r.bottom) * 0.5},
                    now
                });
            }
        }
    }

    for (HWND hwnd : previousFrameWindows) {
        bool stillHere = std::find(
            frameWindows.begin(), frameWindows.end(),
            hwnd) != frameWindows.end();

        if (!stillHere) {
            auto it = previousRects.find(hwnd);

            if (it != previousRects.end()) {
                const RECT& r = it->second;

                closes.push_back({
                    {(r.left + r.right) * 0.5, (r.top + r.bottom) * 0.5},
                    now
                });
            }
        }
    }

    previousFrameWindows = frameWindows;

    constexpr ULONGLONG eventLifetime = 6000;

    std::erase_if(opens, [&](const WindowEvent& e) {
        return now - e.tick > eventLifetime;
    });

    std::erase_if(closes, [&](const WindowEvent& e) {
        return now - e.tick > eventLifetime;
    });
}

const Surface* Desktop::resolve(const Surface& old) const {
    const Surface* result = nullptr;
    double best = 1e30;

    for (const Surface& s : surfaces) {
        if (!s.sameOwner(old)) continue;

        double score =
            dist(s.a, old.a) + dist(s.b, old.b);

        if (score < best) {
            best = score;
            result = &s;
        }
    }

    return result;
}

const Surface* Desktop::nearest(
    V p, double maximum,
    bool horizontalOnly,
    double minimumLength) const
{
    const Surface* result = nullptr;
    double best = maximum;

    for (const Surface& s : surfaces) {
        if (horizontalOnly && std::abs(s.tangent().y) > 0.1) continue;
        if (s.length() < minimumLength) continue;

        double score = segmentDistance(p, s.a, s.b);

        if (score < best) {
            best = score;
            result = &s;
        }
    }

    return result;
}

const Surface* Desktop::groundAt(double x) const {
    for (const Surface& s : surfaces) {
        if (s.ground && x >= s.a.x && x <= s.b.x)
            return &s;
    }

    for (const Surface& s : surfaces)
        if (s.ground) return &s;

    return nullptr;
}

bool Desktop::onScreen(V p) const {
    for (const RECT& r : workAreas) {
        if (p.x >= r.left && p.x <= r.right &&
            p.y >= r.top && p.y <= r.bottom)
            return true;
    }
    return false;
}

bool Desktop::recentlyMoved(HWND owner, double withinSeconds) const {
    auto it = lastMovedTick.find(owner);
    if (it == lastMovedTick.end()) return false;

    ULONGLONG elapsed = GetTickCount64() - it->second;
    return static_cast<double>(elapsed) < withinSeconds * 1000.0;
}

bool Desktop::recentEvent(
    const std::vector<WindowEvent>& events,
    V point, double radius, double withinSeconds)
{
    ULONGLONG now = GetTickCount64();

    for (const WindowEvent& e : events) {
        double elapsed = static_cast<double>(now - e.tick);

        if (elapsed < withinSeconds * 1000.0 &&
            dist(point, e.position) < radius)
            return true;
    }

    return false;
}

bool Desktop::recentWindowOpened(
    V point, double radius, double withinSeconds) const
{
    return recentEvent(opens, point, radius, withinSeconds);
}

bool Desktop::recentWindowClosed(
    V point, double radius, double withinSeconds) const
{
    return recentEvent(closes, point, radius, withinSeconds);
}

void Desktop::emitVisible(
    HWND owner, int edge,
    V a, V b, V normal)
{
    bool horizontal = std::abs(a.y - b.y) < 0.1;
    double low = horizontal ? a.x : a.y;
    double high = horizontal ? b.x : b.y;

    std::vector<std::pair<double, double>> intervals{{low, high}};

    for (const RECT& blocker : blockers) {
        bool crosses = horizontal
            ? a.y >= blocker.top && a.y < blocker.bottom
            : a.x >= blocker.left && a.x < blocker.right;

        if (!crosses) continue;

        double cutA = horizontal ? blocker.left : blocker.top;
        double cutB = horizontal ? blocker.right : blocker.bottom;

        std::vector<std::pair<double, double>> next;

        for (auto [l, r] : intervals) {
            double ca = std::max(l, cutA);
            double cb = std::min(r, cutB);

            if (cb <= ca) {
                next.emplace_back(l, r);
            } else {
                if (ca > l) next.emplace_back(l, ca);
                if (cb < r) next.emplace_back(cb, r);
            }
        }

        intervals = std::move(next);
    }

    for (auto [l, r] : intervals) {
        if (r - l < 95) continue;

        V start = horizontal ? V{l, a.y} : V{a.x, l};
        V end = horizontal ? V{r, a.y} : V{a.x, r};

        surfaces.push_back({
            owner, edge, 0, start, end, normal, false
        });
    }
}

void Desktop::addWindow(HWND hwnd) {
    if (hwnd == ownWindow ||
        !IsWindowVisible(hwnd) ||
        IsIconic(hwnd) ||
        hwnd == GetShellWindow() ||
        hwnd == GetDesktopWindow())
        return;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == GetCurrentProcessId()) return;

    wchar_t className[128]{};
    GetClassNameW(hwnd, className, 128);

    if (lstrcmpW(className, L"Shell_TrayWnd") == 0 ||
        lstrcmpW(className, L"Shell_SecondaryTrayWnd") == 0 ||
        lstrcmpW(className, L"WorkerW") == 0 ||
        lstrcmpW(className, L"Progman") == 0)
        return;

    DWORD cloaked = 0;
    DwmGetWindowAttribute(
        hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));

    if (cloaked) return;

    RECT r{};

    if (FAILED(DwmGetWindowAttribute(
            hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(r))))
        GetWindowRect(hwnd, &r);

    if (r.right <= r.left || r.bottom <= r.top) return;

    auto previous = previousRects.find(hwnd);

    if (previous != previousRects.end()) {
        const RECT& p = previous->second;

        bool moved =
            std::abs(p.left - r.left) > 3 ||
            std::abs(p.top - r.top) > 3 ||
            std::abs(p.right - r.right) > 3 ||
            std::abs(p.bottom - r.bottom) > 3;

        if (moved) lastMovedTick[hwnd] = GetTickCount64();
    }

    previousRects[hwnd] = r;

    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    LONG_PTR exStyle = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);

    bool usable =
        (style & WS_CAPTION) != 0 &&
        (exStyle & WS_EX_TOOLWINDOW) == 0 &&
        r.right - r.left >= 180 &&
        r.bottom - r.top >= 100;

    if (usable) {
        frameWindows.push_back(hwnd);

        emitVisible(
            hwnd, 0,
            {static_cast<double>(r.left), static_cast<double>(r.top)},
            {static_cast<double>(r.right), static_cast<double>(r.top)},
            {0, -1});

        emitVisible(
            hwnd, 1,
            {static_cast<double>(r.left), static_cast<double>(r.top)},
            {static_cast<double>(r.left), static_cast<double>(r.bottom)},
            {-1, 0});

        emitVisible(
            hwnd, 2,
            {static_cast<double>(r.right), static_cast<double>(r.top)},
            {static_cast<double>(r.right), static_cast<double>(r.bottom)},
            {1, 0});
    }

    blockers.push_back(r);
}

} // namespace pets
