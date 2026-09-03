#include <windows.h>
#include <commctrl.h>

#include "halo_audio.h"
#include "halo_engine.h"
#include "halo_ui.h"

/* Define the global patch */
HaloPatch g_current_patch;

#if defined(_MSC_VER)
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "comdlg32.lib")
#pragma comment(linker, "/subsystem:windows")
#endif

#define HALO_CLASS_NAME       "HaloSynthWindow"
#define HALO_WINDOW_TITLE     "halo"
#define HALO_MIN_CLIENT_W     960
#define HALO_MIN_CLIENT_H     640   /* fits four knob rows + 2-octave keyboard */

#define HALO_WINDOW_STYLE     (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_SIZEBOX)
#define HALO_WINDOW_EX_STYLE  (WS_EX_APPWINDOW)
#define HALO_VERSION_STRING   "1.0"

static void enable_dpi_awareness(void) {
    HMODULE hUser32 = GetModuleHandleA("user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextProc setDpiContext = 
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
        if (setDpiContext) {
            setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR lpCmd, int nShow) {
    (void)hPrev;
    (void)lpCmd;

    enable_dpi_awareness();

    INITCOMMONCONTROLSEX icc = { 
        .dwSize = sizeof(icc), 
        .dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES 
    };
    InitCommonControlsEx(&icc);

    audio_init();
    if (g_audio.init_error != 0) {
        char err[160];
        snprintf(err, sizeof(err),
                 "Could not open an audio output device (error %d).\n"
                 "The synth will start silently - check that a playback\n"
                 "device is enabled, then restart halo.",
                 g_audio.init_error);
        MessageBoxA(NULL, err, "Halo Audio", MB_ICONWARNING);
    }

    WNDCLASSEXA wc = {
        .cbSize        = sizeof(WNDCLASSEXA),
        .style         = CS_HREDRAW | CS_VREDRAW,
        .lpfnWndProc   = HaloWndProc,
        .hInstance     = hInst,
        .hCursor       = LoadCursor(NULL, IDC_ARROW),
        .hIcon         = LoadIconA(hInst, MAKEINTRESOURCEA(1)),
        .lpszClassName = HALO_CLASS_NAME
    };

    if (!RegisterClassExA(&wc)) {
        MessageBoxA(NULL, "Failed to register window class.", "Halo Error", MB_ICONERROR);
        audio_shutdown();
        return 1;
    }

    RECT win_rc = { 0, 0, HALO_MIN_CLIENT_W, HALO_MIN_CLIENT_H };
    AdjustWindowRectEx(&win_rc, HALO_WINDOW_STYLE, FALSE, HALO_WINDOW_EX_STYLE);
    int init_w = win_rc.right - win_rc.left;
    int init_h = win_rc.bottom - win_rc.top;

    char windowTitle[64];
    snprintf(windowTitle, sizeof(windowTitle), "halo");

    HWND hwnd = CreateWindowExA(
        HALO_WINDOW_EX_STYLE,
        HALO_CLASS_NAME,
        windowTitle,
        HALO_WINDOW_STYLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        init_w, init_h,
        NULL, NULL, hInst, NULL
    );

    if (!hwnd) {
        MessageBoxA(NULL, "Failed to create application window.", "Halo Error", MB_ICONERROR);
        audio_shutdown();
        return 1;
    }

    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG msg;
    while (GetMessageA(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }

    audio_shutdown();
    return (int)msg.wParam;
}
