#include "application.hpp"

#include <shellapi.h>

#include <cmath>
#include <string>

#pragma comment(lib, "shell32.lib")

namespace pets {

Application::Application()
    : characters{
        std::make_unique<Spider>(),
        std::make_unique<Youxi>(),
        std::make_unique<Plant>()
    }
{
}

LRESULT Application::handle(
    HWND hwnd,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    if (taskbarCreated != 0 && message == taskbarCreated) {
        addTray();
        return 0;
    }

    switch (message) {
    case WM_CREATE:
        initialize(hwnd);
        return 0;

    case WM_TIMER:
        tick();
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;

    case WM_LBUTTONDOWN:
        updateMouse();

        if (picking) {
            int command = pickCommand;
            picking = false;
            pickCommand = 0;

            if (GetCapture() == window)
                ReleaseCapture();

            current().command(command, world.mouse, world);
            render();
            return 0;
        }

        if (!paused && current().down(world.mouse, world)) {
            dragging = true;
            SetCapture(window);
        }

        return 0;

    case WM_LBUTTONUP:
        updateMouse();

        if (dragging) {
            dragging = false;
            current().up(world);
        }

        if (!picking && GetCapture() == window)
            ReleaseCapture();

        return 0;

    case WM_CAPTURECHANGED:
        if (dragging) {
            dragging = false;
            current().up(world);
        }

        return 0;

    case WM_RBUTTONUP:
        if (picking) {
            cancelPick();
        } else {
            showMenu();
        }
        return 0;

    case WM_CONTEXTMENU:
        showMenu();
        return 0;

    case TrayMessage:
        if (lParam == WM_RBUTTONUP ||
            lParam == WM_LBUTTONUP)
            showMenu();
        return 0;

    case WM_COMMAND:
        execute(static_cast<int>(LOWORD(wParam)));
        return 0;

    case WM_DISPLAYCHANGE:
    case WM_SETTINGCHANGE:
        world.desktop.refresh(window);
        return 0;

    case WM_DPICHANGED:
        world.desktop.refresh(window);
        return 0;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT: {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }

    case WM_CLOSE:
        DestroyWindow(window);
        return 0;

    case WM_QUERYENDSESSION:
        save();
        return TRUE;

    case WM_DESTROY:
        KillTimer(window, TimerId);

        if (initialized)
            save();

        removeTray();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

Character& Application::current() {
    return *characters[active];
}

void Application::initialize(HWND hwnd) {
    window = hwnd;

    world.scale = std::clamp(
        settings.get(L"Application", L"scale", 1),
        0.55, 1.8);

    world.palette = std::clamp(
        static_cast<int>(
            settings.get(L"Application", L"palette", 0)),
        0, 2);

    active = static_cast<size_t>(std::clamp(
        static_cast<int>(
            settings.get(L"Application", L"character", 0)),
        0, 2));

    world.audio.enabled =
        settings.get(L"Application", L"sound", 0) != 0;

    hideFullscreen =
        settings.get(L"Application", L"hideFullscreen", 1) != 0;

    world.desktop.refresh(window);
    updateMouse();
    world.previousMouse = world.mouse;

    for (auto& character : characters)
        character->initialize(world, settings);

    renderer.initialize(window);

    taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&previousCounter);

    addTray();

    initialized = true;

    render();
    SetTimer(window, TimerId, 16, nullptr);
}

void Application::updateMouse() {
    POINT p{};
    GetCursorPos(&p);

    world.mouse = {
        static_cast<double>(p.x),
        static_cast<double>(p.y)
    };
}

void Application::tick() {
    LARGE_INTEGER now{};
    QueryPerformanceCounter(&now);

    double elapsed =
        static_cast<double>(
            now.QuadPart - previousCounter.QuadPart) /
        static_cast<double>(frequency.QuadPart);

    previousCounter = now;

    updateMouse();

    desktopClock += elapsed;
    saveClock += elapsed;

    if (desktopClock >= 1.0 / 30.0) {
        desktopClock = 0;
        world.desktop.refresh(window);
    }

    if (saveClock >= 15) {
        saveClock = 0;
        save();
    }

    if (picking && (GetAsyncKeyState(VK_ESCAPE) & 0x8000))
        cancelPick();

    bool shouldHide =
        hideFullscreen && !picking && !dragging &&
        foregroundFullscreen();

    if (shouldHide != currentlyHidden) {
        currentlyHidden = shouldHide;

        ShowWindow(
            window,
            shouldHide ? SW_HIDE : SW_SHOWNOACTIVATE);
    }

    if (!paused && !currentlyHidden) {
        accumulator += std::min(elapsed, 0.08);

        bool firstStep = true;

        while (accumulator >= STEP) {
            if (!firstStep)
                world.previousMouse = world.mouse;

            current().tick(world, STEP);

            accumulator -= STEP;
            firstStep = false;
        }
    } else {
        accumulator = 0;
    }

    world.previousMouse = world.mouse;

    if (!currentlyHidden)
        render();
}

void Application::render() {
    Scene scene;
    current().draw(scene, world);

    if (picking) {
        V p = world.mouse;
        double s = world.scale;

        scene.ring(
            p, 12 * s, 12 * s,
            color(0xDAE7BC, 0.9f), 1.5 * s);

        scene.line(
            p + V{-19 * s, 0},
            p + V{19 * s, 0},
            color(0xDCE9CD, 0.8f), s);

        scene.line(
            p + V{0, -19 * s},
            p + V{0, 19 * s},
            color(0xDCE9CD, 0.8f), s);
    }

    renderer.draw(
        scene,
        picking,
        world.desktop.virtualRect);
}

bool Application::foregroundFullscreen() const {
    HWND foreground = GetForegroundWindow();

    if (!foreground ||
        foreground == window ||
        foreground == GetShellWindow() ||
        foreground == GetDesktopWindow())
        return false;

    DWORD pid = 0;
    GetWindowThreadProcessId(foreground, &pid);

    if (pid == GetCurrentProcessId())
        return false;

    wchar_t className[128]{};
    GetClassNameW(foreground, className, 128);

    if (lstrcmpW(className, L"Progman") == 0 ||
        lstrcmpW(className, L"WorkerW") == 0 ||
        lstrcmpW(className, L"Shell_TrayWnd") == 0)
        return false;

    LONG_PTR style =
        GetWindowLongPtrW(foreground, GWL_STYLE);

    // 普通最大化窗口仍然属于角色活动环境。
    if ((style & WS_CAPTION) == WS_CAPTION)
        return false;

    RECT r{};
    GetWindowRect(foreground, &r);

    HMONITOR monitor = MonitorFromWindow(
        foreground, MONITOR_DEFAULTTONEAREST);

    MONITORINFO info{};
    info.cbSize = sizeof(info);
    GetMonitorInfoW(monitor, &info);

    return r.left <= info.rcMonitor.left &&
           r.top <= info.rcMonitor.top &&
           r.right >= info.rcMonitor.right &&
           r.bottom >= info.rcMonitor.bottom;
}

void Application::beginPick(int command) {
    if (dragging) {
        dragging = false;
        current().up(world);
    }

    pickCommand = command;
    picking = true;
    paused = false;

    if (currentlyHidden) {
        currentlyHidden = false;
        ShowWindow(window, SW_SHOWNOACTIVATE);
    }

    render();
    SetCapture(window);
}

void Application::cancelPick() {
    picking = false;
    pickCommand = 0;

    if (GetCapture() == window)
        ReleaseCapture();

    render();
}

void Application::execute(int command) {
    if (command >= ChooseSpider && command <= ChoosePlant) {
        current().up(world);

        dragging = false;
        picking = false;

        if (GetCapture() == window)
            ReleaseCapture();

        active = static_cast<size_t>(command - ChooseSpider);
        accumulator = 0;

        updateTrayTip();
        save();
        render();
        return;
    }

    switch (command) {
    case Inspect:
    case Summon:
    case PlaceHome:
        beginPick(command);
        return;

    case ReturnHome:
    case Gather:
    case Expand:
        current().command(command, world.mouse, world);
        return;

    case TogglePause:
        if (dragging) {
            dragging = false;
            current().up(world);
            ReleaseCapture();
        }

        paused = !paused;
        accumulator = 0;
        updateTrayTip();
        return;

    case ToggleSound:
        world.audio.enabled = !world.audio.enabled;

        if (world.audio.enabled)
            world.audio.play(static_cast<int>(active), 480, 0.1);

        save();
        return;

    case ToggleFullscreen:
        hideFullscreen = !hideFullscreen;
        save();
        return;

    case SizeSmall:
        world.scale = 0.75;
        break;

    case SizeNormal:
        world.scale = 1;
        break;

    case SizeLarge:
        world.scale = 1.25;
        break;

    case SizeExtra:
        world.scale = 1.5;
        break;

    case PaletteOne:
    case PaletteTwo:
    case PaletteThree:
        world.palette = command - PaletteOne;
        break;

    case Quit:
        DestroyWindow(window);
        return;

    default:
        return;
    }

    save();
    render();
}

void Application::showMenu() {
    if (picking)
        cancelPick();

    HMENU menu = CreatePopupMenu();
    HMENU sizeMenu = CreatePopupMenu();
    HMENU paletteMenu = CreatePopupMenu();

    auto item = [](HMENU target, UINT id,
                   const wchar_t* title, bool checked = false)
    {
        AppendMenuW(
            target,
            MF_STRING | (checked ? MF_CHECKED : 0),
            id,
            title);
    };

    item(menu, ChooseSpider, L"机械蜘蛛", active == 0);
    item(menu, ChooseYouxi, L"游隙", active == 1);
    item(menu, ChoosePlant, L"栖枝", active == 2);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    if (active == 0) {
        item(menu, Inspect, L"检查这里…");
    } else if (active == 1) {
        item(menu, Summon, L"到这里来…");
        item(menu, ReturnHome, L"回巢");
        item(menu, PlaceHome, L"在这里结巢…");
    } else {
        item(menu, Gather, L"收拢枝蔓");
        item(menu, Expand, L"舒展与攀援");
    }

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

    item(sizeMenu, SizeSmall, L"75%",
         std::abs(world.scale - 0.75) < 0.01);

    item(sizeMenu, SizeNormal, L"100%",
         std::abs(world.scale - 1) < 0.01);

    item(sizeMenu, SizeLarge, L"125%",
         std::abs(world.scale - 1.25) < 0.01);

    item(sizeMenu, SizeExtra, L"150%",
         std::abs(world.scale - 1.5) < 0.01);

    const wchar_t* paletteNames[3][3]{
        {L"冷灰工业", L"砂色工具机", L"深色合金"},
        {L"晨雾", L"暮潮", L"琥珀"},
        {L"青瓷", L"赤霞", L"夜紫"}
    };

    for (int i = 0; i < 3; ++i) {
        item(
            paletteMenu,
            PaletteOne + i,
            paletteNames[active][i],
            world.palette == i);
    }

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(sizeMenu),
        L"大小");

    AppendMenuW(
        menu,
        MF_POPUP,
        reinterpret_cast<UINT_PTR>(paletteMenu),
        L"配色");

    item(
        menu,
        ToggleSound,
        L"角色声音",
        world.audio.enabled);

    item(
        menu,
        ToggleFullscreen,
        L"全屏应用时隐藏",
        hideFullscreen);

    item(
        menu,
        TogglePause,
        paused ? L"恢复活动" : L"暂停活动",
        paused);

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    item(menu, Quit, L"退出");

    POINT point{};
    GetCursorPos(&point);

    SetForegroundWindow(window);

    int selected = TrackPopupMenu(
        menu,
        TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY,
        point.x,
        point.y,
        0,
        window,
        nullptr);

    DestroyMenu(menu);

    PostMessageW(window, WM_NULL, 0, 0);

    if (selected)
        execute(selected);
}

void Application::addTray() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);

    data.hWnd = window;
    data.uID = 1;

    data.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    data.uCallbackMessage = TrayMessage;
    data.hIcon = LoadIconW(nullptr, IDI_APPLICATION);

    std::wstring tip =
        std::wstring(L"Desktop Pets · ") + current().name();

    wcsncpy_s(data.szTip, tip.c_str(), _TRUNCATE);

    Shell_NotifyIconW(NIM_ADD, &data);
}

void Application::updateTrayTip() {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);

    data.hWnd = window;
    data.uID = 1;
    data.uFlags = NIF_TIP;

    std::wstring tip =
        std::wstring(L"Desktop Pets · ") +
        current().name() +
        (paused ? L" · 暂停" : L"");

    wcsncpy_s(data.szTip, tip.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &data);
}

void Application::removeTray() const {
    NOTIFYICONDATAW data{};
    data.cbSize = sizeof(data);
    data.hWnd = window;
    data.uID = 1;

    Shell_NotifyIconW(NIM_DELETE, &data);
}

void Application::save() const {
    settings.set(
        L"Application", L"character",
        static_cast<double>(active));

    settings.set(L"Application", L"scale", world.scale);
    settings.set(L"Application", L"palette", world.palette);
    settings.set(L"Application", L"sound", world.audio.enabled ? 1 : 0);
    settings.set(
        L"Application", L"hideFullscreen",
        hideFullscreen ? 1 : 0);

    for (const auto& character : characters)
        character->save(settings);
}

} // namespace pets
