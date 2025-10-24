#ifndef MYC23PROJECT_WIN32_PLATFORM_H
#define MYC23PROJECT_WIN32_PLATFORM_H

#include <windows.h>
#include "renderer.h"

// Contains all the Win32-specific graphics handles and information.
typedef struct {
    BITMAPINFO info;
    HDC device_context;
} win32_graphics_context;

void win32_resize_dib_section(graphics_buffer *buffer, uint32_t width, uint32_t height);

void win32_display_buffer(
    const graphics_buffer *buffer,
    HDC device_context,
    uint32_t window_width,
    uint32_t window_height
);

#endif //MYC23PROJECT_WIN32_PLATFORM_H
