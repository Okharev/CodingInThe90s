#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static bool running = false;

struct win32_bitmap_dimensions {
    uint32_t width;
    uint32_t height;
};

static struct win32_bitmap_dimensions win32_get_bitmap_dimensions(HWND window) {
    RECT rect;
    GetClientRect(window, &rect);

    struct win32_bitmap_dimensions dims;
    dims.width = rect.right - rect.left;
    dims.height = rect.bottom - rect.top;

    return dims;
}

struct win32_screen_bitmap_buffer {
    BITMAPINFO bitmapinfo;
    void * restrict bitmapbuff;
};

static struct win32_screen_bitmap_buffer bitmap_buff;

static void RenderStuff(const uint32_t XOffset, const uint32_t YOffset) {
    constexpr uint32_t bytes_per_pixel = 4;
    const uint32_t pitch = bytes_per_pixel * bitmap_buff.bitmapinfo.bmiHeader.biWidth;
    uint8_t * restrict row = __builtin_assume_aligned(bitmap_buff.bitmapbuff, 32);

    for (int y = 0; y < bitmap_buff.bitmapinfo.bmiHeader.biHeight; ++y) {
        // pixel color format: 00 RR GG BB
        uint32_t * restrict pixel = __builtin_assume_aligned(row, 16);

        for (int x = 0; x < bitmap_buff.bitmapinfo.bmiHeader.biWidth; ++x) {
            const uint8_t blue = x + XOffset;
            const uint8_t green = y + YOffset;
            constexpr uint8_t red = 0x80;

            *pixel++ = red << 16 | green << 8 | blue;
        }

        row += pitch;
    }
}

static void ResizeDIBSection(const uint32_t width, const uint32_t height) {
    if (bitmap_buff.bitmapbuff) {
        VirtualFree(bitmap_buff.bitmapbuff, 0, MEM_RELEASE);
    }

    bitmap_buff.bitmapinfo.bmiHeader.biSize = sizeof(bitmap_buff.bitmapinfo.bmiHeader);
    bitmap_buff.bitmapinfo.bmiHeader.biWidth = width;
    bitmap_buff.bitmapinfo.bmiHeader.biHeight = height;
    bitmap_buff.bitmapinfo.bmiHeader.biPlanes = 1;
    bitmap_buff.bitmapinfo.bmiHeader.biBitCount = 32;
    bitmap_buff.bitmapinfo.bmiHeader.biCompression = BI_RGB;

    constexpr uint32_t bytes_per_pixel = 4;
    bitmap_buff.bitmapbuff = VirtualAlloc(
        nullptr,
        width * height * bytes_per_pixel,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
}

static void RenderLoop(
    HDC hdc,
    const uint32_t width,
    const uint32_t height
) {
    StretchDIBits(
        hdc,
        0,
        0,
        bitmap_buff.bitmapinfo.bmiHeader.biWidth,
        bitmap_buff.bitmapinfo.bmiHeader.biHeight,
        0,
        0,
        width,
        height,
        bitmap_buff.bitmapbuff,
        &bitmap_buff.bitmapinfo,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

LRESULT CALLBACK MainWndProc(
    HWND wnd,
    const UINT msg,
    const WPARAM wparam,
    const LPARAM lparam
) {
    LRESULT res = 0;

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            const HDC hdc = BeginPaint(wnd, &ps);

            const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(wnd);
            RenderLoop(hdc, dims.width, dims.height);

            EndPaint(wnd, &ps);
        }
        break;
        case WM_SIZE: {
            const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(wnd);
            ResizeDIBSection(dims.width, dims.height);
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
    running = true;

    int x = 0; int y = 0;
    while (running) {
        while (PeekMessageA(&msg, win_handle, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        RenderStuff(x, y);

        const HDC hdc = GetDC(win_handle);
        const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(win_handle);

        RenderLoop(hdc, dims.width, dims.height);

        ReleaseDC(win_handle, hdc);
        ++x;
        ++y;
    }

    return 0;
}
