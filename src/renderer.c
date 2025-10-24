#include "renderer.h"

#include <string.h>

#define OUTCODE_RIGHT  1  // 000001
#define OUTCODE_LEFT   2  // 000010
#define OUTCODE_TOP    4  // 000100
#define OUTCODE_BOTTOM 8  // 001000
#define OUTCODE_FAR    16 // 010000
#define OUTCODE_NEAR   32 // 100000

static inline int32_t compute_outcode(const vec3 v) {
    int code = 0;
    if (v[0] > 1.0f) code |= OUTCODE_RIGHT;
    if (v[0] < -1.0f) code |= OUTCODE_LEFT;
    if (v[1] > 1.0f) code |= OUTCODE_TOP;
    if (v[1] < -1.0f) code |= OUTCODE_BOTTOM;
    if (v[2] > 1.0f) code |= OUTCODE_FAR;
    if (v[2] < -1.0f) code |= OUTCODE_NEAR;
    return code;
}

static inline void get_cam_view_mat4(camera cam, mat4 dest) {
    mat4 rotation_mat = GLM_MAT4_IDENTITY_INIT;
    mat4 translate_mat = GLM_MAT4_IDENTITY_INIT;

    glm_quat_rotate(rotation_mat, cam.rotation, rotation_mat);
    glm_translate(translate_mat, cam.position);
    glm_mul(translate_mat, rotation_mat, dest);
    glm_inv_tr(dest);
}

static inline int iabs(const int x) { return x >= 0 ? x : -x; }
static inline int isgn(const int x) { return (x > 0) - (x < 0); }
static inline void swap_int(int *a, int *b) {
    const int temp = *a;
    *a = *b;
    *b = temp;
}

void draw_line(
    const graphics_buffer *restrict buff,
    int x0, int y0,
    int x1, int y1,
    const uint8_t r,
    const uint8_t g,
    const uint8_t b
) {
    // --- Fast path for horizontal lines ---
    if (y0 == y1) {
        if (x0 > x1) swap_int(&x0, &x1);
        for (int x = x0; x <= x1; x++) {
            set_pixel(buff, x, y0, r, g, b);
        }
        return;
    }

    // --- Fast path for vertical lines ---
    if (x0 == x1) {
        if (y0 > y1) swap_int(&y0, &y1);
        for (int y = y0; y <= y1; y++) {
            set_pixel(buff, x0, y, r, g, b);
        }
        return;
    }

    // --- Unified Bresenham's algorithm for all other cases ---
    const int dx = iabs(x1 - x0);
    const int dy = -iabs(y1 - y0); // dy is negative
    const int incX = (x0 < x1) ? 1 : -1;
    const int incY = (y0 < y1) ? 1 : -1;
    int error = dx + dy; // Initial error value

    while (1) {
        set_pixel(buff, x0, y0, r, g, b);

        // Check if the end point has been reached
        if (x0 == x1 && y0 == y1) break;

        const int e2 = 2 * error;
        if (e2 >= dy) { // Favor moving in X
            error += dy;
            x0 += incX;
        }
        if (e2 <= dx) { // Favor moving in Y
            error += dx;
            y0 += incY;
        }
    }
}

void set_pixel(
    const graphics_buffer * restrict buffer,
    const uint32_t x,
    const uint32_t y,
    const uint8_t r,
    const uint8_t g,
    const uint8_t b
) {
    if (x >= buffer->width || y >= buffer->height) {
        return;
    }

    uint32_t * restrict pixels = buffer->memory;
    pixels[y * buffer->width + x] = (r << 16) | (g << 8) | b;
}

void model_build_unique_edges(model *m) {
    uint32_t capacity = m->index_count;
    model_edge * restrict unique_edges = malloc(sizeof(model_edge) * capacity);
    uint32_t unique_count = 0;

    for (uint32_t i = 0; i < m->index_count; i += 3) {
        const uint32_t indices[] = {m->indices[i], m->indices[i+1], m->indices[i+2]};
        const model_edge triangle_edges[3] = {{indices[0], indices[1]}, {indices[1], indices[2]}, {indices[2], indices[0]}};

        for (int j = 0; j < 3; j++) {
            // Canonical representation (smaller index first) to ensure (v0, v1) is the same as (v1, v0)
            const uint32_t v_start = triangle_edges[j].v0 < triangle_edges[j].v1 ? triangle_edges[j].v0 : triangle_edges[j].v1;
            const uint32_t v_end   = triangle_edges[j].v0 > triangle_edges[j].v1 ? triangle_edges[j].v0 : triangle_edges[j].v1;

            bool found = false;
            for (uint32_t k = 0; k < unique_count; k++) {
                if (unique_edges[k].v0 == v_start && unique_edges[k].v1 == v_end) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                if (unique_count >= capacity) {
                    capacity *= 2;
                    unique_edges = realloc(unique_edges, sizeof(model_edge) * capacity);
                }
                unique_edges[unique_count++] = (model_edge){v_start, v_end};
            }
        }
    }

    m->edge_count = unique_count;
    m->edges = malloc(sizeof(model_edge) * m->edge_count);
    if (m->edges) {
        memcpy(m->edges, unique_edges, sizeof(model_edge) * m->edge_count);
    }
    free(unique_edges);
}


void render_obj_wire(model model, vec3 pos, versor rot, vec3 scale, camera *restrict cam, graphics_buffer *restrict buff) {
    mat4 model_matrix; {
        mat4 rotation_mat = GLM_MAT4_IDENTITY_INIT;
        mat4 translate_mat = GLM_MAT4_IDENTITY_INIT;
        mat4 scale_mat = GLM_MAT4_IDENTITY_INIT;

        // Create individual transform matrices
        glm_quat_rotate(rotation_mat, rot, rotation_mat);
        glm_translate_make(translate_mat, pos);
        glm_scale_make(scale_mat, scale);

        // Combine them in standard T * R * S order
        mat4 rs_mat;
        glm_mul(rotation_mat, scale_mat, rs_mat);
        glm_mul(translate_mat, rs_mat, model_matrix);
    }

    mat4 mvp_mat;
    glm_mat4_mul(*camera_get_pv_matrix(cam), model_matrix, mvp_mat);

    // Pre-calculate loop-invariant values for the viewport transform
    const float half_width = 0.5f * (float) buff->width;
    const float half_height = 0.5f * (float) buff->height;

    for (uint32_t i = 0; i < model.edge_count; ++i) {
        // Get vertex indices for the current edge
        const uint32_t i0 = model.edges[i].v0;
        const uint32_t i1 = model.edges[i].v1;

        // Transform vertices to Clip Space
        vec4 clip_v0, clip_v1;
        glm_mat4_mulv(mvp_mat, model.vertices[i0], clip_v0);
        glm_mat4_mulv(mvp_mat, model.vertices[i1], clip_v1);

        // Simple Near-Plane Culling for the line segment
        if (clip_v0[3] <= 0.0f || clip_v1[3] <= 0.0f) {
            continue;
        }

        // Perspective Divide
        vec3 ndc_v0, ndc_v1;
        glm_vec3_divs((vec3){clip_v0[0], clip_v0[1], clip_v0[2]}, clip_v0[3], ndc_v0);
        glm_vec3_divs((vec3){clip_v1[0], clip_v1[1], clip_v1[2]}, clip_v1[3], ndc_v1);

        // Viewport Transform
        const int sx0 = (ndc_v0[0] + 1.0f) * half_width;
        const int sy0 = (1.0f - ndc_v0[1]) * half_height;
        const int sx1 = (ndc_v1[0] + 1.0f) * half_width;
        const int sy1 = (1.0f - ndc_v1[1]) * half_height;

        draw_line(buff, sx0, sy0, sx1, sy1, 0xFF, 0xFF, 0xFF);
    }
}

void render_obj(model model, vec3 pos, versor rot, vec3 scale, camera *restrict cam, graphics_buffer *restrict buff) {
    // --- 1. Calculate the Model-View-Projection (MVP) matrix (once per object) ---
    mat4 model_matrix; {
        mat4 rotation_mat = GLM_MAT4_IDENTITY_INIT;
        mat4 translate_mat = GLM_MAT4_IDENTITY_INIT;
        mat4 scale_mat = GLM_MAT4_IDENTITY_INIT;

        // Create individual transform matrices
        glm_quat_rotate(rotation_mat, rot, rotation_mat);
        glm_translate_make(translate_mat, pos);
        glm_scale_make(scale_mat, scale);

        // Combine them in standard T * R * S order
        mat4 rs_mat;
        glm_mul(rotation_mat, scale_mat, rs_mat);
        glm_mul(translate_mat, rs_mat, model_matrix);
    }

    mat4 mvp_mat;
    glm_mat4_mul(*camera_get_pv_matrix(cam), model_matrix, mvp_mat);

    // Pre-calculate loop-invariant values for the viewport transform
    const float half_width = 0.5f * (float) buff->width;
    const float half_height = 0.5f * (float) buff->height;

    // --- 2. Process each triangle of the model ---
    for (int i = 0; i < model.index_count; i += 3) {
        const uint32_t i0 = model.indices[i];
        const uint32_t i1 = model.indices[i + 1];
        const uint32_t i2 = model.indices[i + 2];

        // --- 2a. Transform vertices from Model Space to Clip Space ---
        vec4 clip_v0, clip_v1, clip_v2;
        glm_mat4_mulv(mvp_mat, model.vertices[i0], clip_v0);
        glm_mat4_mulv(mvp_mat, model.vertices[i1], clip_v1);
        glm_mat4_mulv(mvp_mat, model.vertices[i2], clip_v2);

        // --- 2b. CRITICAL: Near-Plane Culling ---
        // Discard any triangle with a vertex behind or on the camera's near plane.
        if (clip_v0[3] <= 0.0f || clip_v1[3] <= 0.0f || clip_v2[3] <= 0.0f) {
            continue;
        }

        // --- 2c. Perspective Divide (to Normalized Device Coordinates) ---
        vec3 ndc_v0, ndc_v1, ndc_v2; {
            // Use reciprocal multiplication (faster than division)
            const float recip_w0 = 1.0f / clip_v0[3];
            ndc_v0[0] = clip_v0[0] * recip_w0;
            ndc_v0[1] = clip_v0[1] * recip_w0;
            ndc_v0[2] = clip_v0[2] * recip_w0;

            const float recip_w1 = 1.0f / clip_v1[3];
            ndc_v1[0] = clip_v1[0] * recip_w1;
            ndc_v1[1] = clip_v1[1] * recip_w1;
            ndc_v1[2] = clip_v1[2] * recip_w1;

            const float recip_w2 = 1.0f / clip_v2[3];
            ndc_v2[0] = clip_v2[0] * recip_w2;
            ndc_v2[1] = clip_v2[1] * recip_w2;
            ndc_v2[2] = clip_v2[2] * recip_w2;
        }

        // --- 2d. OPTIMIZATION: Back-Face Culling ---
        // Discard triangles that are facing away from the camera.
        {
            vec3 edge1, edge2;
            glm_vec3_sub(ndc_v1, ndc_v0, edge1);
            glm_vec3_sub(ndc_v2, ndc_v0, edge2);
            vec3 normal;
            glm_vec3_cross(edge1, edge2, normal);
            if (normal[2] > 0.0f) {
                // Positive Z in NDC is away from the camera
                continue;
            }
        }

        // --- 2e. OPTIMIZATION: Frustum Culling (Trivial Rejection) ---
        // Discard triangles that are entirely outside the viewing volume.
        {
            const int outcode0 = compute_outcode(ndc_v0);
            const int outcode1 = compute_outcode(ndc_v1);
            const int outcode2 = compute_outcode(ndc_v2);

            // If the bitwise AND is non-zero, all 3 vertices are outside the same plane.
            if ((outcode0 & outcode1 & outcode2) != 0) {
                continue;
            }
        }

        // --- 2f. Viewport Transform (NDC to Screen Coordinates) ---
        vec3 screen_v0, screen_v1, screen_v2; {
            // Map X from [-1, 1] to [0, screen_width]
            screen_v0[0] = (ndc_v0[0] + 1.0f) * half_width;
            screen_v1[0] = (ndc_v1[0] + 1.0f) * half_width;
            screen_v2[0] = (ndc_v2[0] + 1.0f) * half_width;

            // Map Y from [-1, 1] to [screen_height, 0] (inverting Y for top-left origin)
            screen_v0[1] = (1.0f - ndc_v0[1]) * half_height;
            screen_v1[1] = (1.0f - ndc_v1[1]) * half_height;
            screen_v2[1] = (1.0f - ndc_v2[1]) * half_height;

            // Map Z from [-1, 1] to [0, 1] for the depth buffer
            screen_v0[2] = (ndc_v0[2] + 1.0f) * 0.5f;
            screen_v1[2] = (ndc_v1[2] + 1.0f) * 0.5f;
            screen_v2[2] = (ndc_v2[2] + 1.0f) * 0.5f;
        }

        draw_line(buff, screen_v0[0], screen_v0[1], screen_v1[0], screen_v1[1], 0xFF, 0x00, 0x00);
        draw_line(buff, screen_v1[0], screen_v1[1], screen_v2[0], screen_v2[1], 0x00, 0xFF, 0x00);
        draw_line(buff, screen_v2[0], screen_v2[1], screen_v0[0], screen_v0[1], 0x00, 0x00, 0xFF);
    }
}

void clean_buff(const graphics_buffer * restrict buffer) {
    memset(buffer->memory, 0, buffer->pitch * buffer->height);
}

mat4 const *camera_get_pv_matrix(camera *restrict cam) {
    const bool needs_pv_recalc = cam->view_is_dirty || cam->projection_is_dirty;

    if (cam->view_is_dirty) {
        mat4 rotation_mat = GLM_MAT4_IDENTITY_INIT;
        mat4 translation_mat = GLM_MAT4_IDENTITY_INIT;

        glm_quat_rotate(rotation_mat, cam->rotation, rotation_mat);
        glm_translate_make(translation_mat, cam->position);

        mat4 transform;
        glm_mul(translation_mat, rotation_mat, transform);

        glm_mat4_inv(transform, cam->view_mat);

        cam->view_is_dirty = false;
    }

    if (cam->projection_is_dirty) {
        glm_perspective(cam->FOV, cam->aspect, cam->near_clip, cam->far_clip, cam->proj_mat);

        cam->projection_is_dirty = false;
    }

    if (needs_pv_recalc) {
        glm_mat4_mul(cam->proj_mat, cam->view_mat, cam->pv_mat);
    }

    return &cam->pv_mat;
}

void camera_init(
    camera *cam,
    vec3 pos,
    versor rot,
    const float fov,
    const float aspect,
    const float near,
    const float far
) {
    glm_vec3_copy(pos, cam->position);
    glm_quat_copy(rot, cam->rotation);

    cam->FOV = fov;
    cam->aspect = aspect;
    cam->near_clip = near;
    cam->far_clip = far;

    cam->view_is_dirty = true;
    cam->projection_is_dirty = true;
}

void render_gradient(const graphics_buffer *restrict buffer, const uint32_t x_offset, const uint32_t y_offset) {
    uint8_t *restrict row = buffer->memory;
    for (uint32_t y = 0; y < buffer->height; ++y) {
        uint32_t *restrict pixel = (uint32_t * restrict) row;
        for (uint32_t x = 0; x < buffer->width; ++x) {
            const uint8_t blue = x + x_offset;
            const uint8_t green = y + y_offset;
            constexpr uint8_t red = 0x80;

            *pixel++ = red << 16 | green << 8 | blue;
        }
        row += buffer->pitch;
    }
}
