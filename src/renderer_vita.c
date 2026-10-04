#include "kh1vita/renderer.h"
#include "kh1vita/config.h"
#include "kh1vita/software_rasterizer.h"

#include <math.h>
#include <psp2/display.h>
#include <psp2/kernel/sysmem.h>
#include <string.h>
#include <stdlib.h>

#define FB_BYTES (KH1VITA_SCREEN_PITCH * KH1VITA_SCREEN_HEIGHT * 4)
#define ALIGN_UP(v, a) (((v) + ((a) - 1)) & ~((a) - 1))

static SceUID g_fb_uid = -1;
static uint32_t *g_fb = 0;
static float *g_depth = 0;

static void fill_rect(int x, int y, int w, int h, uint32_t rgba) {
    if (!g_fb) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > KH1VITA_SCREEN_WIDTH) w = KH1VITA_SCREEN_WIDTH - x;
    if (y + h > KH1VITA_SCREEN_HEIGHT) h = KH1VITA_SCREEN_HEIGHT - y;
    if (w <= 0 || h <= 0) return;
    for (int yy = y; yy < y + h; ++yy) {
        uint32_t *row = g_fb + yy * KH1VITA_SCREEN_PITCH + x;
        for (int xx = 0; xx < w; ++xx) row[xx] = rgba;
    }
}

static void put_pixel(int x, int y, uint32_t color) {
    if (!g_fb) return;
    if ((unsigned)x >= KH1VITA_SCREEN_WIDTH || (unsigned)y >= KH1VITA_SCREEN_HEIGHT) return;
    g_fb[y * KH1VITA_SCREEN_PITCH + x] = color;
}

static int abs_i(int x) { return x < 0 ? -x : x; }

static void draw_line(int x0, int y0, int x1, int y1, uint32_t color) {
    int dx = abs_i(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs_i(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int guard = 0;

    /* Guard against corrupt geometry producing an effectively unbounded line. */
    while (guard++ < 4096) {
        put_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        {
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
}

int kh1vita_renderer_init(void) {
    const unsigned alloc_size = ALIGN_UP(FB_BYTES, 256 * 1024);
    g_fb_uid = sceKernelAllocMemBlock("kh1vita_fb", SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW,
                                     alloc_size, NULL);
    if (g_fb_uid < 0) return -1;
    if (sceKernelGetMemBlockBase(g_fb_uid, (void **)&g_fb) < 0 || !g_fb) return -2;

    memset(g_fb, 0, FB_BYTES);
    g_depth = (float *)malloc(KH1VITA_SCREEN_WIDTH * KH1VITA_SCREEN_HEIGHT * sizeof(*g_depth));
    if (!g_depth) {
        sceKernelFreeMemBlock(g_fb_uid);
        g_fb_uid = -1;
        g_fb = 0;
        return -3;
    }
    SceDisplayFrameBuf fb;
    memset(&fb, 0, sizeof(fb));
    fb.size = sizeof(fb);
    fb.base = g_fb;
    fb.pitch = KH1VITA_SCREEN_PITCH;
    fb.pixelformat = SCE_DISPLAY_PIXELFORMAT_A8B8G8R8;
    fb.width = KH1VITA_SCREEN_WIDTH;
    fb.height = KH1VITA_SCREEN_HEIGHT;
    sceDisplaySetFrameBuf(&fb, SCE_DISPLAY_SETBUF_NEXTFRAME);
    sceDisplayWaitVblankStart();
    return 0;
}

void kh1vita_renderer_shutdown(void) {
    free(g_depth);
    g_depth = 0;
    if (g_fb_uid >= 0) {
        sceKernelFreeMemBlock(g_fb_uid);
        g_fb_uid = -1;
        g_fb = 0;
    }
}

void kh1vita_renderer_status(int elf_ok, int kingdom_ok, int fatal_error) {
    const uint32_t BG = 0xFF171717u;
    const uint32_t BLUE = 0xFFD06020u;
    const uint32_t GREEN = 0xFF37A05Bu;
    const uint32_t ORANGE = 0xFF3090D0u;
    const uint32_t RED = 0xFF3040D0u;

    fill_rect(0, 0, KH1VITA_SCREEN_WIDTH, KH1VITA_SCREEN_HEIGHT, BG);
    fill_rect(0, 0, KH1VITA_SCREEN_WIDTH, 72, fatal_error ? RED : BLUE);
    fill_rect(90, 150, 780, 90, elf_ok ? GREEN : RED);
    fill_rect(90, 300, 780, 90, kingdom_ok ? GREEN : ORANGE);
    fill_rect(90, 445, 780, 18, (elf_ok && kingdom_ok && !fatal_error) ? GREEN : ORANGE);
    sceDisplayWaitVblankStart();
}

void kh1vita_renderer_wireframe(const Kh1VitaRenderVertex *vertices,
                                uint32_t vertex_count,
                                const Kh1VitaRenderTriangle *triangles,
                                uint32_t triangle_count,
                                float yaw_radians) {
    const uint32_t BG = 0xFF101010u;
    const float center_y = 76.29f;
    const float scale = 3.0f;
    const float screen_cx = 480.0f;
    const float screen_cy = 276.0f;
    float c, s;
    uint32_t i;

    if (!g_fb || !vertices || !triangles) return;
    c = cosf(yaw_radians);
    s = sinf(yaw_radians);
    fill_rect(0, 0, KH1VITA_SCREEN_WIDTH, KH1VITA_SCREEN_HEIGHT, BG);

    for (i = 0; i < triangle_count; ++i) {
        const Kh1VitaRenderTriangle *t = &triangles[i];
        const Kh1VitaRenderVertex *v[3];
        int sx[3], sy[3];
        unsigned k;
        if (t->a >= vertex_count || t->b >= vertex_count || t->c >= vertex_count) continue;
        v[0] = &vertices[t->a];
        v[1] = &vertices[t->b];
        v[2] = &vertices[t->c];
        for (k = 0; k < 3u; ++k) {
            float rx = c * v[k]->x + s * v[k]->z;
            float ry = v[k]->y - center_y;
            sx[k] = (int)(screen_cx + rx * scale);
            sy[k] = (int)(screen_cy - ry * scale);
        }
        draw_line(sx[0], sy[0], sx[1], sy[1], t->color);
        draw_line(sx[1], sy[1], sx[2], sy[2], t->color);
        draw_line(sx[2], sy[2], sx[0], sy[0], t->color);
    }

    /* Tiny progress/status strip: green means geometry is live. */
    fill_rect(0, KH1VITA_SCREEN_HEIGHT - 5, KH1VITA_SCREEN_WIDTH, 5, 0xFF37A05Bu);
    sceDisplayWaitVblankStart();
}

void kh1vita_renderer_textured(const Kh1VitaRenderVertex *vertices,
                               uint32_t vertex_count,
                               const Kh1VitaRenderTriangle *triangles,
                               uint32_t triangle_count,
                               const Kh1VitaRenderTexture *textures,
                               uint32_t texture_count,
                               float yaw_radians) {
    Kh1SwView view;
    if (!g_fb || !g_depth || !vertices || !triangles || !textures) return;
    view.center_y = 76.29f;
    view.scale = 3.0f;
    view.screen_cx = 480.0f;
    view.screen_cy = 276.0f;
    view.yaw_radians = yaw_radians;
    kh1_sw_clear(g_fb, KH1VITA_SCREEN_WIDTH, KH1VITA_SCREEN_HEIGHT,
                 KH1VITA_SCREEN_PITCH, g_depth, 0xFF101010u);
    kh1_sw_draw_model(g_fb, KH1VITA_SCREEN_WIDTH, KH1VITA_SCREEN_HEIGHT,
                      KH1VITA_SCREEN_PITCH, g_depth,
                      vertices, vertex_count, triangles, triangle_count,
                      textures, texture_count, &view);
    fill_rect(0, KH1VITA_SCREEN_HEIGHT - 5, KH1VITA_SCREEN_WIDTH, 5, 0xFF37A05Bu);
    sceDisplayWaitVblankStart();
}
