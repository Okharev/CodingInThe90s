#include "win32_platform.h"

void win32_resize_dib_section(graphics_buffer *buffer, const uint32_t width, const uint32_t height) {
    if (buffer->memory) {
        VirtualFree(buffer->memory, 0, MEM_RELEASE);
    }

    buffer->width = width;
    buffer->height = height;

    // Negative height to make the DIB top-down
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = width;
    info.bmiHeader.biHeight = -(int32_t) height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    const uint32_t bytes_per_pixel = 4;
    buffer->pitch = width * bytes_per_pixel;

    buffer->memory = VirtualAlloc(0, buffer->pitch * height, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void win32_display_buffer(const graphics_buffer *buffer, HDC device_context, const uint32_t window_width,
                          const uint32_t window_height) {
    BITMAPINFO info = {0};
    info.bmiHeader.biSize = sizeof(info.bmiHeader);
    info.bmiHeader.biWidth = buffer->width;
    info.bmiHeader.biHeight = -(int32_t) buffer->height;
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;

    StretchDIBits(
        device_context,
        0, 0, window_width, window_height,
        0, 0, buffer->width, buffer->height,
        buffer->memory,
        &info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}
