#pragma once

#include "kh1vita/renderer.h"
#include <stdint.h>

typedef struct Kh1SwView {
    float center_y;
    float scale;
    float screen_cx;
    float screen_cy;
    float yaw_radians;
} Kh1SwView;

/* RGBA framebuffer uses 0xAABBGGRR, matching SCE_DISPLAY_PIXELFORMAT_A8B8G8R8. */
void kh1_sw_clear(uint32_t *framebuffer, uint32_t width, uint32_t height,
                  uint32_t pitch, float *depth, uint32_t color);
void kh1_sw_draw_model(uint32_t *framebuffer, uint32_t width, uint32_t height,
                       uint32_t pitch, float *depth,
                       const Kh1VitaRenderVertex *vertices, uint32_t vertex_count,
                       const Kh1VitaRenderTriangle *triangles, uint32_t triangle_count,
                       const Kh1VitaRenderTexture *textures, uint32_t texture_count,
                       const Kh1SwView *view);
