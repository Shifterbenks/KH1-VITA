#pragma once
#include <stdint.h>

typedef struct Kh1VitaRenderVertex {
    float x;
    float y;
    float z;
    float u;
    float v;
} Kh1VitaRenderVertex;

typedef struct Kh1VitaRenderTriangle {
    uint32_t a;
    uint32_t b;
    uint32_t c;
    uint32_t texture_index;
    uint32_t color;
} Kh1VitaRenderTriangle;

typedef struct Kh1VitaRenderTexture {
    uint32_t width;
    uint32_t height;
    const uint32_t *pixels;
} Kh1VitaRenderTexture;

int kh1vita_renderer_init(void);
void kh1vita_renderer_shutdown(void);
void kh1vita_renderer_status(int elf_ok, int kingdom_ok, int fatal_error);
void kh1vita_renderer_wireframe(const Kh1VitaRenderVertex *vertices,
                                uint32_t vertex_count,
                                const Kh1VitaRenderTriangle *triangles,
                                uint32_t triangle_count,
                                float yaw_radians);
void kh1vita_renderer_textured(const Kh1VitaRenderVertex *vertices,
                               uint32_t vertex_count,
                               const Kh1VitaRenderTriangle *triangles,
                               uint32_t triangle_count,
                               const Kh1VitaRenderTexture *textures,
                               uint32_t texture_count,
                               float yaw_radians);
