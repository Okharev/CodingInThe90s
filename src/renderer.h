#ifndef MYC23PROJECT_RENDERER_H
#define MYC23PROJECT_RENDERER_H

#include <stdint.h>

#include "cglm/cglm.h"

typedef struct {
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    void *memory;
} graphics_buffer;

typedef struct {
    uint32_t v0;
    uint32_t v1;
} model_edge;

typedef struct {
    vec4 *vertices;
    uint32_t vertex_count;
    uint32_t *indices;
    uint32_t index_count;

    // --- NEW MEMBERS ---
    model_edge *edges;       // A dynamic array of unique edges
    uint32_t edge_count;     // The number of unique edges
} model;

typedef struct {
    vec3 position;
    versor rotation;

    float FOV;
    float aspect;
    float near_clip;
    float far_clip;

    mat4 view_mat;
    mat4 proj_mat;
    mat4 pv_mat;

    bool view_is_dirty;
    bool projection_is_dirty;
} camera;

void render_obj_wire(model model, vec3 pos, versor rot, vec3 scale, camera *restrict cam, graphics_buffer *restrict buff);

void model_build_unique_edges(model * restrict m);

void clean_buff(const graphics_buffer *restrict buffer);

mat4 const *camera_get_pv_matrix(camera *restrict cam);

void camera_init(camera *cam, vec3 pos, versor rot, float fov, float aspect, float near, float far);

void render_gradient(const graphics_buffer *restrict buffer, uint32_t x_offset, uint32_t y_offset);

void set_pixel(
    const graphics_buffer * restrict buffer,
    uint32_t x,
    uint32_t y,
    uint8_t r,
    uint8_t g,
    uint8_t b
);

void render_obj(model model, vec3 pos, versor rot, vec3 scale, camera *restrict cam, graphics_buffer *restrict buff);

#endif //MYC23PROJECT_RENDERER_H
