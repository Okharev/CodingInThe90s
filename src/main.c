#include <windows.h>
#include "renderer.h"
#include "win32_platform.h"

static bool g_running = FALSE;
static graphics_buffer g_backbuffer;

LRESULT CALLBACK main_window_proc(HWND wnd, const UINT msg, const WPARAM w_param, const LPARAM l_param) {
    switch (msg) {
        case WM_SIZE: {
            RECT rect;
            GetClientRect(wnd, &rect);
            win32_resize_dib_section(&g_backbuffer, rect.right - rect.left, rect.bottom - rect.top);
            return 0;
        }
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(wnd, &ps);
            RECT rect;
            GetClientRect(wnd, &rect);
            win32_display_buffer(&g_backbuffer, hdc, rect.right - rect.left, rect.bottom - rect.top);
            EndPaint(wnd, &ps);
            return 0;
        }
        case WM_CLOSE:
        case WM_DESTROY: {
            g_running = false;
            return 0;
        }
        default: {
            return DefWindowProcA(wnd, msg, w_param, l_param);
        }
    }
}

void init_cube_mesh(model *cube_model) {
    constexpr uint32_t CUBE_VERTEX_COUNT = 36;
    cube_model->vertex_size = CUBE_VERTEX_COUNT;

    const vec3 corners[8] = {
        {-0.5f, -0.5f, 0.5f},
        {0.5f, -0.5f, 0.5f},
        {0.5f, 0.5f, 0.5f},
        {-0.5f, 0.5f, 0.5f},
        {0.5f, -0.5f, -0.5f},
        {-0.5f, -0.5f, -0.5f},
        {-0.5f, 0.5f, -0.5f},
        {0.5f, 0.5f, -0.5f}
    };

    const vec4 vertices[CUBE_VERTEX_COUNT] = {
        // Front Face
        {corners[0][0], corners[0][1], corners[0][2], 1.0f},
        {corners[1][0], corners[1][1], corners[1][2], 1.0f},
        {corners[2][0], corners[2][1], corners[2][2], 1.0f},
        {corners[2][0], corners[2][1], corners[2][2], 1.0f},
        {corners[3][0], corners[3][1], corners[3][2], 1.0f},
        {corners[0][0], corners[0][1], corners[0][2], 1.0f},

        // Back Face
        {corners[4][0], corners[4][1], corners[4][2], 1.0f},
        {corners[5][0], corners[5][1], corners[5][2], 1.0f},
        {corners[6][0], corners[6][1], corners[6][2], 1.0f},
        {corners[6][0], corners[6][1], corners[6][2], 1.0f},
        {corners[7][0], corners[7][1], corners[7][2], 1.0f},
        {corners[4][0], corners[4][1], corners[4][2], 1.0f},

        // Left Face
        {corners[5][0], corners[5][1], corners[5][2], 1.0f},
        {corners[0][0], corners[0][1], corners[0][2], 1.0f},
        {corners[3][0], corners[3][1], corners[3][2], 1.0f},
        {corners[3][0], corners[3][1], corners[3][2], 1.0f},
        {corners[6][0], corners[6][1], corners[6][2], 1.0f},
        {corners[5][0], corners[5][1], corners[5][2], 1.0f},

        // Right Face
        {corners[1][0], corners[1][1], corners[1][2], 1.0f},
        {corners[4][0], corners[4][1], corners[4][2], 1.0f},
        {corners[7][0], corners[7][1], corners[7][2], 1.0f},
        {corners[7][0], corners[7][1], corners[7][2], 1.0f},
        {corners[2][0], corners[2][1], corners[2][2], 1.0f},
        {corners[1][0], corners[1][1], corners[1][2], 1.0f},

        // Top Face
        {corners[3][0], corners[3][1], corners[3][2], 1.0f},
        {corners[2][0], corners[2][1], corners[2][2], 1.0f},
        {corners[7][0], corners[7][1], corners[7][2], 1.0f},
        {corners[7][0], corners[7][1], corners[7][2], 1.0f},
        {corners[6][0], corners[6][1], corners[6][2], 1.0f},
        {corners[3][0], corners[3][1], corners[3][2], 1.0f},

        // Bottom Face
        {corners[5][0], corners[5][1], corners[5][2], 1.0f},
        {corners[4][0], corners[4][1], corners[4][2], 1.0f},
        {corners[1][0], corners[1][1], corners[1][2], 1.0f},
        {corners[1][0], corners[1][1], corners[1][2], 1.0f},
        {corners[0][0], corners[0][1], corners[0][2], 1.0f},
        {corners[5][0], corners[5][1], corners[5][2], 1.0f}
    };

    cube_model->vertex = malloc(sizeof(vec4) * CUBE_VERTEX_COUNT);

    if (cube_model->vertex) {
        memcpy(cube_model->vertex, vertices, sizeof(vec4) * CUBE_VERTEX_COUNT);
    }
}

void init_camera_for_cube(camera *cam, float window_width, float window_height) {
    constexpr vec3 cam_pos = {5.0f, 5.0f, 5.0f};
    memcpy(cam->position, cam_pos, sizeof(vec3));
    glm_quat_forp(cam_pos, GLM_VEC3_ZERO, ((vec3){0.0f, 0.0f, 1.0f}), cam->rotation);

    cam->FOV = 45.0f;
    cam->aspect = window_width / window_height;
    cam->near_clip = 0.1f;
    cam->far_clip = 100.0f;
    cam->projection_is_dirty = true;
    cam->view_is_dirty = true;
}

int WINAPI WinMain(HINSTANCE instance, [[maybe_unused]] HINSTANCE hPrevInstance, [[maybe_unused]] PSTR lpCmdLine,
                   [[maybe_unused]] int nShowCmd) {
    WNDCLASSA window_class = {0};
    window_class.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    window_class.lpfnWndProc = main_window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = "CExampleWindowClass";

    if (!RegisterClassA(&window_class)) {
        return 1;
    }

    HWND window = CreateWindowExA(
        0, window_class.lpszClassName, "C renderer",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        CW_USEDEFAULT, CW_USEDEFAULT,
        0, 0, instance, 0
    );

    if (!window) {
        return 1;
    }

    g_running = true;

    HDC hdc = GetDC(window);

    model my_cube;
    init_cube_mesh(&my_cube);

    camera my_camera;
    init_camera_for_cube(&my_camera, g_backbuffer.width, g_backbuffer.height);

    while (g_running) {
        MSG msg;
        while (PeekMessageA(&msg, 0, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = false;
            }
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        render_obj(my_cube, GLM_VEC3_ONE, GLM_QUAT_IDENTITY, GLM_VEC3_ONE, &my_camera, &g_backbuffer);

        RECT rect;
        GetClientRect(window, &rect);
        win32_display_buffer(&g_backbuffer, hdc, rect.right - rect.left, rect.bottom - rect.top);
        ReleaseDC(window, hdc);
    }

    return 0;
}
