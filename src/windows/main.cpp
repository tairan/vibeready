#include <windows.h>

namespace {

constexpr wchar_t kWindowClassName[] = L"VibeReadyMainWindow";
constexpr wchar_t kWindowTitle[] = L"VibeReady";

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    switch (message) {
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_PAINT: {
        PAINTSTRUCT paint;
        HDC dc = BeginPaint(hwnd, &paint);

        RECT rect;
        GetClientRect(hwnd, &rect);

        const wchar_t title[] = L"VibeReady";
        const wchar_t body[] = L"Windows portable client skeleton";
        const wchar_t note[] = L"No installer. No service. No background process.";

        SetBkMode(dc, TRANSPARENT);
        DrawTextW(dc, title, -1, &rect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        RECT bodyRect = rect;
        bodyRect.top += 64;
        DrawTextW(dc, body, -1, &bodyRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        RECT noteRect = rect;
        noteRect.top += 104;
        DrawTextW(dc, note, -1, &noteRect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        EndPaint(hwnd, &paint);
        return 0;
    }

    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

bool RegisterMainWindowClass(HINSTANCE instance) {
    WNDCLASSEXW windowClass = {};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    windowClass.lpszClassName = kWindowClassName;

    return RegisterClassExW(&windowClass) != 0;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand) {
    if (!RegisterMainWindowClass(instance)) {
        MessageBoxW(nullptr, L"VibeReady could not start.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    HWND window = CreateWindowExW(
        0,
        kWindowClassName,
        kWindowTitle,
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        420,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!window) {
        MessageBoxW(nullptr, L"VibeReady could not create its main window.", kWindowTitle, MB_ICONERROR | MB_OK);
        return 1;
    }

    ShowWindow(window, showCommand);
    UpdateWindow(window);

    MSG message = {};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    return static_cast<int>(message.wParam);
}
