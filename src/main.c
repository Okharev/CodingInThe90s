#include <stdint.h>
#include <stdio.h>
#include <windows.h>
#include "cglm/cglm.h"

static bool running = false;

struct win32_bitmap_dimensions {
    uint32_t width;
    uint32_t height;
};

union pixel {
    alignas(4) uint32_t pixel;

    alignas(4) struct {
        uint8_t B;
        uint8_t G;
        uint8_t R;
        uint8_t P;
    } __attribute__((packed));

    alignas(4) uint8_t BGRP[4];
    __attribute__((aligned(4))) uint8_t *restrict raw_BGRP;
};

static struct win32_bitmap_dimensions win32_get_bitmap_dimensions(const HWND window) {
    RECT rect;
    GetClientRect(window, &rect);

    struct win32_bitmap_dimensions dims;
    dims.width = rect.right - rect.left;
    dims.height = rect.bottom - rect.top;

    return dims;
}

static constexpr uint32_t bytes_per_pixel = 4;

struct win32_screen_bitmap_buffer {
    uint32_t pitch;
    BITMAPINFO bitmapinfo;
    void *restrict bitmapbuff;
};

static struct win32_screen_bitmap_buffer bitmap_buff;

static bool is_aligned(const void *restrict ptr, const size_t alignment) {
    return (uintptr_t) ptr % alignment == 0;
}

static bool is_power_of_two(const uintptr_t x) {
    return (x & x - 1) == 0;
}

static void clean_buff() {
    memset(bitmap_buff.bitmapbuff, 0, bitmap_buff.bitmapinfo.bmiHeader.biWidth * bitmap_buff.bitmapinfo.bmiHeader.biHeight * bytes_per_pixel);
}

static void set_pixel(const uint32_t x, const uint32_t y, const uint8_t R, const uint8_t G, const uint8_t B) {
    uint32_t *restrict pixel = bitmap_buff.bitmapbuff;
    pixel[y * bitmap_buff.pitch + x] = R << 16 | G << 8 | B;
}

static void render_line(int32_t x0, int32_t y0, const int32_t x1, const int32_t y1) {
    const int32_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int32_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;

    while (true) {
        set_pixel(x0, y0, 0xFF, 0x00, 0x00);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void render_gradient(const uint32_t x_offset, const uint32_t y_offset) {
    uint8_t *restrict row = bitmap_buff.bitmapbuff;

    for (uint32_t y = 0; y < bitmap_buff.bitmapinfo.bmiHeader.biHeight; ++y) {
        // pixel color format: 00 RR GG BB
        uint32_t *restrict pixel = (uint32_t * restrict) row;

        for (uint32_t x = 0; x < bitmap_buff.bitmapinfo.bmiHeader.biWidth; ++x) {
            const uint8_t blue = x + x_offset;
            const uint8_t green = y + y_offset;
            constexpr uint8_t red = 0x80;

            *pixel++ = red << 16 | green << 8 | blue;
        }

        row += bitmap_buff.pitch;
    }
}

static void resize_DIB_section(const uint32_t width, const uint32_t height) {
    if (bitmap_buff.bitmapbuff) {
        VirtualFree(bitmap_buff.bitmapbuff, 0, MEM_RELEASE);
    }

    bitmap_buff.bitmapinfo.bmiHeader.biSize = sizeof(bitmap_buff.bitmapinfo.bmiHeader);
    bitmap_buff.bitmapinfo.bmiHeader.biWidth = width;
    bitmap_buff.bitmapinfo.bmiHeader.biHeight = height;
    bitmap_buff.bitmapinfo.bmiHeader.biPlanes = 1;
    bitmap_buff.bitmapinfo.bmiHeader.biBitCount = 32;
    bitmap_buff.bitmapinfo.bmiHeader.biCompression = BI_RGB;
    bitmap_buff.pitch = bytes_per_pixel * width;

    bitmap_buff.bitmapbuff = VirtualAlloc(
        nullptr,
        width * height * bytes_per_pixel,
        MEM_COMMIT | MEM_RESERVE,
        PAGE_READWRITE
    );
}

static void stretch_wnd(
    const HDC hdc,
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
    const HWND wnd,
    const UINT msg,
    const WPARAM wparam,
    const LPARAM lparam
) {
    LRESULT res = 0;

    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(wnd, &ps);

            const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(wnd);
            stretch_wnd(hdc, dims.width, dims.height);

            EndPaint(wnd, &ps);
        }
        break;
        case WM_SIZE: {
            const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(wnd);
            resize_DIB_section(dims.width, dims.height);
        }
        break;
        case WM_CLOSE:
        case WM_QUIT: {
            running = false;
        }
        break;

        default: {
            res = DefWindowProcA(wnd, msg, wparam, lparam);
        }
        break;
    }

    return res;
}

static void move_line(ivec2* point1) {
    if (bitmap_buff.bitmapinfo.bmiHeader.biWidth < *point1[0]) {
        *point1[0] = *point1[0] + 1;
    }
    if (bitmap_buff.bitmapinfo.bmiHeader.biHeight < *point1[0]) {
        *point1[1] = *point1[1] + 1;
    }
}

int WINAPI WinMain(
    const HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    PSTR lpCmdLine,
    int nShowCmd
) {
    WNDCLASS WindowClass = {0};

    WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    WindowClass.lpfnWndProc = MainWndProc;
    WindowClass.hInstance = hInstance;
    WindowClass.lpszClassName = "Window class";

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

    HDC hdc = GetDC(win_handle);

    MSG msg;
    running = true;

    ivec2 point0 = {0, 0};
    ivec2 point1 = {0, 0};

    const struct win32_bitmap_dimensions dims = win32_get_bitmap_dimensions(win_handle);

    while (running) {
        while (PeekMessageA(&msg, win_handle, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }
        move_line(&point1);


        clean_buff();
        render_line(point0[0], point0[1], point1[0], point1[1]);

        stretch_wnd(hdc, dims.width, dims.height);
        ReleaseDC(win_handle, hdc);
    }

    return 0;
}
