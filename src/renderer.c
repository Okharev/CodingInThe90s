#include "renderer.h"

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

void set_pixel(const graphics_buffer *restrict buffer, const uint32_t x, const uint32_t y, const uint8_t r,
               const uint8_t g, const uint8_t b) {
    if ((x < 0 || x > buffer->width) || (y < 0 || y > buffer->height)) {
        return;
    }

    uint32_t *pixel = (uint32_t *) ((uint8_t *) buffer->memory + y * buffer->pitch + x * sizeof(uint32_t));
    *pixel = (r << 16) | (g << 8) | b;
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
    for (int i = 0; i < model.vertex_size; i += 3) {
        // --- 2a. Transform vertices from Model Space to Clip Space ---
        vec4 clip_v0, clip_v1, clip_v2;
        glm_mat4_mulv(mvp_mat, model.vertex[i], clip_v0);
        glm_mat4_mulv(mvp_mat, model.vertex[i + 1], clip_v1);
        glm_mat4_mulv(mvp_mat, model.vertex[i + 2], clip_v2);

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

        // --- 3. Pass to Rasterizer ---
        // drawing/filling function.
        set_pixel(buff, (uint32_t) screen_v0[0], (uint32_t) screen_v0[1], 0xFF, 0xFF, 0x00);
        set_pixel(buff, (uint32_t) screen_v1[0], (uint32_t) screen_v1[1], 0xFF, 0xFF, 0x00);
        set_pixel(buff, (uint32_t) screen_v2[0], (uint32_t) screen_v2[1], 0xFF, 0xFF, 0x00);
    }
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

void camera_init(camera *cam, vec3 pos, versor rot, const float fov, const float aspect, const float near,
                 const float far) {
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
