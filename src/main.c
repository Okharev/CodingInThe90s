#include <stdio.h>
#include <windows.h>
#include "cglm/cglm.h"
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
            win32_display_buffer(
                &g_backbuffer,
                hdc,
                rect.right - rect.left,
                rect.bottom - rect.top
            );
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
    constexpr uint32_t UNIQUE_VERTEX_COUNT = 8;
    const vec4 unique_vertices[UNIQUE_VERTEX_COUNT] = {
        {-0.5f, -0.5f, 0.5f, 1.0f}, {0.5f, -0.5f, 0.5f, 1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}, {-0.5f, 0.5f, 0.5f, 1.0f},
        {-0.5f, -0.5f, -0.5f, 1.0f}, {0.5f, -0.5f, -0.5f, 1.0f}, {0.5f, 0.5f, -0.5f, 1.0f}, {-0.5f, 0.5f, -0.5f, 1.0f}
    };
    constexpr uint32_t CUBE_INDEX_COUNT = 36;
    const uint32_t cube_indices[CUBE_INDEX_COUNT] = {
        0, 2, 1, 0, 3, 2, 5, 7, 4, 5, 6, 7, 4, 3, 0, 4, 7, 3,
        5, 2, 6, 5, 1, 2, 3, 6, 2, 3, 7, 6, 0, 1, 5, 0, 5, 4
    };

    cube_model->vertex_count = UNIQUE_VERTEX_COUNT;
    cube_model->index_count = CUBE_INDEX_COUNT;
    cube_model->vertices = malloc(sizeof(vec4) * UNIQUE_VERTEX_COUNT);
    cube_model->indices = malloc(sizeof(uint32_t) * CUBE_INDEX_COUNT);
    memcpy(cube_model->vertices, unique_vertices, sizeof(vec4) * UNIQUE_VERTEX_COUNT);
    memcpy(cube_model->indices, cube_indices, sizeof(uint32_t) * CUBE_INDEX_COUNT);

    // Call the pre-processing function
    model_build_unique_edges(cube_model);
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

int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE hPrevInstance,
    PSTR lpCmdLine,
    int nShowCmd
) {
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

    mat4 cube_rot = GLM_MAT4_IDENTITY_INIT;

    vec3 cube_pos = GLM_VEC3_ZERO_INIT;
    vec3 velocity = {0.001f, 0.001f, 0.001f};

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

        if (cube_pos[0] > 3.0f || cube_pos[0] < -3.0f) {
            velocity[0] = -velocity[0];
        }
        if (cube_pos[1] > 3.0f || cube_pos[1] < -3.0f) {
            velocity[1] = -velocity[1];
        }
        if (cube_pos[2] > 3.0f || cube_pos[2] < -3.0f) {
            velocity[2] = -velocity[2];
        }

        glm_rotate_y(cube_rot, 0.001f, cube_rot);
        glm_rotate_x(cube_rot, 0.001f, cube_rot);

        clean_buff(&g_backbuffer);

        versor rot;
        glm_mat4_quat(cube_rot, rot);
        render_obj_raster(my_cube, cube_pos, rot, GLM_VEC3_ONE, &my_camera, &g_backbuffer);

        glm_vec3_add(cube_pos, velocity, cube_pos);

        RECT rect;
        GetClientRect(window, &rect);
        win32_display_buffer(
            &g_backbuffer,
            hdc,
            rect.right - rect.left,
            rect.bottom - rect.top
        );
        ReleaseDC(window, hdc);
    }

    return 0;
}
