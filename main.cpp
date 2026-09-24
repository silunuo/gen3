#include "application.hpp"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

namespace pets {

LRESULT CALLBACK windowProcedure(
    HWND window,
    UINT message,
    WPARAM wParam,
    LPARAM lParam)
{
    auto* application =
        reinterpret_cast<Application*>(
            GetWindowLongPtrW(window, GWLP_USERDATA));

    if (message == WM_NCCREATE) {
        const auto* create =
            reinterpret_cast<CREATESTRUCTW*>(lParam);

        application =
            static_cast<Application*>(create->lpCreateParams);

        SetWindowLongPtrW(
            window,
            GWLP_USERDATA,
            reinterpret_cast<LONG_PTR>(application));
    }

    try {
        if (application) {
            return application->handle(
                window, message, wParam, lParam);
        }
    } catch (const std::exception& error) {
        KillTimer(window, 1);

        MessageBoxA(
            window,
            error.what(),
            "Desktop Pets",
            MB_OK | MB_ICONERROR);

        if (message == WM_CREATE)
            return -1;

        if (message != WM_DESTROY)
            DestroyWindow(window);

        return 0;
    }

    return DefWindowProcW(
        window, message, wParam, lParam);
}

} // namespace pets

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int)
{
    SetProcessDpiAwarenessContext(
        DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HRESULT initializedCOM = CoInitializeEx(
        nullptr, COINIT_APARTMENTTHREADED);

    int result = 0;

    {
        pets::Application application;

        constexpr wchar_t className[] =
            L"DesktopPets.CompositionHost";

        WNDCLASSEXW windowClass{};
        windowClass.cbSize = sizeof(windowClass);

        windowClass.hInstance = instance;
        windowClass.lpfnWndProc = pets::windowProcedure;

        windowClass.hCursor =
            LoadCursorW(nullptr, IDC_ARROW);

        windowClass.hIcon =
            LoadIconW(nullptr, IDI_APPLICATION);

        windowClass.lpszClassName = className;

        RegisterClassExW(&windowClass);

        DWORD extendedStyle =
            WS_EX_TOPMOST |
            WS_EX_TOOLWINDOW |
            WS_EX_NOACTIVATE |
            WS_EX_NOREDIRECTIONBITMAP;

        HWND window = CreateWindowExW(
            extendedStyle,
            className,
            L"Desktop Pets",
            WS_POPUP,
            0, 0, 64, 64,
            nullptr,
            nullptr,
            instance,
            &application);

        if (!window) {
            result = 1;
        } else {
            ShowWindow(window, SW_SHOWNOACTIVATE);

            MSG message{};
            BOOL status = 0;

            while ((status = GetMessageW(
                        &message, nullptr, 0, 0)) > 0)
            {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }

            result = status == -1
                ? 1
                : static_cast<int>(message.wParam);
        }
    }

    if (SUCCEEDED(initializedCOM))
        CoUninitialize();

    return result;
}
