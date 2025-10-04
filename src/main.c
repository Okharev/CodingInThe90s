#include <stdio.h>
#include <windows.h>

static bool running = false;

static void ResizeDIBSection(size_t width, size_t height) {
    printf("new width, height: %llu, %llu", width, height);
    // auto buff = CreateDIBSection();
}

LRESULT CALLBACK MainWndProc(
    HWND wnd, // handle to window
    const UINT msg, // message identifier
    const WPARAM wparam, // first message parameter
    const LPARAM lparam) // second message parameter
{
    LRESULT res = 0;

    switch (msg) {
        case WM_PAINT: {
            // printf("WM_PAINT");

            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(wnd, &ps);
            static DWORD oper = BLACKNESS;

            const int x = ps.rcPaint.left;
            const int y = ps.rcPaint.top;
            const int width = ps.rcPaint.right - ps.rcPaint.left;
            const int height = ps.rcPaint.bottom - ps.rcPaint.top;

            PatBlt(hdc, x, y, width, height, oper);

            if (oper == BLACKNESS) {
                oper = WHITENESS;
            } else {
                oper = BLACKNESS;
            }

            EndPaint(wnd, &ps);
        }
        break;
        case WM_SIZE: {

            RECT rect;
            GetClientRect(wnd, &rect);
            const size_t width = rect.right - rect.left;
            const size_t height = rect.bottom - rect.top;

            ResizeDIBSection(width, height);
            printf("WM_SIZE");
        }
        break;

        case WM_CLOSE: {
            printf("WM_CLOSE");

            running = false;
        }
        break;
        case WM_QUIT: {
            printf("WM_QUIT");

            running = false;
        }
        break;

        case WM_DESTROY: {
            printf("WM_DESTROY");
        }
        break;

        case WM_ACTIVATEAPP: {
            printf("WM_ACTIVATEAPP");
        }
        break;

        default: {
            res = DefWindowProcA(wnd, msg, wparam, lparam);
        }
        break;
    }

    return res;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nShowCmd) {
    WNDCLASS WindowClass = {};
    running = true;

    WindowClass.lpfnWndProc = MainWndProc;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "Yeey";

    if (!RegisterClassA(&WindowClass)) {
        return 1;
    }

    HWND win_handle = CreateWindowExA(
        0,
        WindowClass.lpszClassName,
        "HandMade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        nullptr,
        nullptr,
        hInstance,
        nullptr
    );

    if (!win_handle) {
        return 0;
    }

    MSG msg;
    while (running) {
        // If a message is in the queue.
        if (PeekMessageA(&msg, win_handle, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
    }

    return 0;
}
