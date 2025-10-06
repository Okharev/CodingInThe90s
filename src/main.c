#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static bool running = false;

static BITMAPINFO bitmapinfo;
static void *bitmapbuff;
static uint32_t bitmap_width;
static uint32_t bitmap_height;

static void RenderStuff(const uint32_t XOffset, const uint32_t YOffset) {
    constexpr uint32_t bytes_per_pixel = 4;
    const uint32_t pitch = bytes_per_pixel * bitmap_width;
    uint8_t *row = bitmapbuff;
    for (int y = 0; y < bitmap_height; ++y) {
        // pixel color format: 00 RR GG BB
        uint32_t *pixel = row;

        for (int x = 0; x < bitmap_width; ++x) {
            const uint8_t blue = x + XOffset;
            const uint8_t green = y + YOffset;
            constexpr uint8_t red = 0x80;

            *pixel++ = red << 16 | green << 8 | blue;
        }

        row += pitch;
    }
}

static void ResizeDIBSection(const uint32_t width, const uint32_t height) {
    printf("new width, height: %d, %d", width, height);

    if (bitmapbuff) {
        VirtualFree(bitmapbuff, 0, MEM_RELEASE);
    }

    bitmap_width = width;
    bitmap_height = height;

    bitmapinfo.bmiHeader.biSize = sizeof(bitmapinfo.bmiHeader);
    bitmapinfo.bmiHeader.biWidth = bitmap_width;
    bitmapinfo.bmiHeader.biHeight = bitmap_height;
    bitmapinfo.bmiHeader.biPlanes = 1;
    bitmapinfo.bmiHeader.biBitCount = 32;
    bitmapinfo.bmiHeader.biCompression = BI_RGB;

    constexpr uint32_t bytes_per_pixel = 4;
    bitmapbuff = VirtualAlloc(
        nullptr,
        width * height * bytes_per_pixel,
        MEM_COMMIT,
        PAGE_READWRITE
    );
}

static void RenderLoop(
    HDC hdc,
    const RECT *winrect,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height
) {
    const uint32_t window_width = winrect->right - winrect->left;
    const uint32_t window_height = winrect->bottom - winrect->top;

    StretchDIBits(
        hdc,
        0,
        0,
        bitmap_width,
        bitmap_height,
        0,
        0,
        window_width,

        window_height,
        bitmapbuff,
        &bitmapinfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

LRESULT CALLBACK MainWndProc(HWND wnd, const UINT msg, const WPARAM wparam,
                             const LPARAM lparam) {
    LRESULT res = 0;

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            const HDC hdc = BeginPaint(wnd, &ps);

            const uint32_t x = ps.rcPaint.left;
            const uint32_t y = ps.rcPaint.top;
            const uint32_t width = ps.rcPaint.right - ps.rcPaint.left;
            const uint32_t height = ps.rcPaint.bottom - ps.rcPaint.top;

            RECT rect;
            GetClientRect(wnd, &rect);

            RenderLoop(hdc, &rect, x, y, width, height);

            EndPaint(wnd, &ps);
        }
        break;
        case WM_SIZE: {
            RECT rect;
            GetClientRect(wnd, &rect);
            const uint32_t width = rect.right - rect.left;
            const uint32_t height = rect.bottom - rect.top;
            ResizeDIBSection(width, height);
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

int WINAPI WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PSTR lpCmdLine,
    int nShowCmd
) {
    WNDCLASS WindowClass = {};

    WindowClass.lpfnWndProc = MainWndProc;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "Yeey";

    if (!RegisterClassA(&WindowClass)) {
        return 1;
    }

    const HWND win_handle = CreateWindowExA(
        0, WindowClass.lpszClassName, "HandMade Hero",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, hInstance, nullptr);

    if (!win_handle) {
        return 0;
    }

    MSG msg;
    running = true;
    uint32_t x = 0;
    uint32_t y = 0;
    while (running) {
        while (PeekMessageA(&msg, win_handle, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        RenderStuff(x, y);
        const HDC hdc = GetDC(win_handle);
        RECT rect;
        GetClientRect(win_handle, &rect);
        const uint32_t window_width = rect.right - rect.left;
        const uint32_t window_height = rect.bottom - rect.top;
        RenderLoop(hdc, &rect, 0, 0, window_width, window_height);
        ReleaseDC(win_handle, hdc);
        ++x;
        ++y;
    }

    return 0;
}
