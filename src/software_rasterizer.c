#include "kh1vita/software_rasterizer.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

static float edge(float ax, float ay, float bx, float by, float px, float py) {
    return (px - ax) * (by - ay) - (py - ay) * (bx - ax);
}

static int clamp_i(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static float clamp_f(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint32_t alpha_blend(uint32_t dst, uint32_t src) {
    unsigned a = (src >> 24) & 255u;
    unsigned inv;
    unsigned sr, sg, sb, dr, dg, db;
    if (a == 255u) return src;
    if (a == 0u) return dst;
    inv = 255u - a;
    sr = src & 255u;
    sg = (src >> 8) & 255u;
    sb = (src >> 16) & 255u;
    dr = dst & 255u;
    dg = (dst >> 8) & 255u;
    db = (dst >> 16) & 255u;
    return ((sr * a + dr * inv) / 255u)
         | (((sg * a + dg * inv) / 255u) << 8)
         | (((sb * a + db * inv) / 255u) << 16)
         | 0xff000000u;
}

void kh1_sw_clear(uint32_t *framebuffer, uint32_t width, uint32_t height,
                  uint32_t pitch, float *depth, uint32_t color) {
    uint32_t y, x;
    if (!framebuffer || !depth || pitch < width) return;
    for (y = 0; y < height; ++y) {
        uint32_t *row = framebuffer + y * pitch;
        float *zrow = depth + y * width;
        for (x = 0; x < width; ++x) {
            row[x] = color;
            zrow[x] = -FLT_MAX;
        }
    }
}

void kh1_sw_draw_model(uint32_t *framebuffer, uint32_t width, uint32_t height,
                       uint32_t pitch, float *depth,
                       const Kh1VitaRenderVertex *vertices, uint32_t vertex_count,
                       const Kh1VitaRenderTriangle *triangles, uint32_t triangle_count,
                       const Kh1VitaRenderTexture *textures, uint32_t texture_count,
                       const Kh1SwView *view) {
    float c, s;
    uint32_t ti;
    if (!framebuffer || !depth || !vertices || !triangles || !textures || !view ||
        width == 0u || height == 0u || pitch < width) return;

    c = cosf(view->yaw_radians);
    s = sinf(view->yaw_radians);

    for (ti = 0; ti < triangle_count; ++ti) {
        const Kh1VitaRenderTriangle *t = &triangles[ti];
        const Kh1VitaRenderTexture *tex;
        const Kh1VitaRenderVertex *v[3];
        float sx[3], sy[3], sz[3];
        float area, sign, inv_area;
        float minxf, maxxf, minyf, maxyf;
        int minx, maxx, miny, maxy, y, x;
        unsigned k;

        if (t->a >= vertex_count || t->b >= vertex_count || t->c >= vertex_count ||
            t->texture_index >= texture_count) continue;
        tex = &textures[t->texture_index];
        if (!tex->pixels || tex->width == 0u || tex->height == 0u) continue;
        v[0] = &vertices[t->a];
        v[1] = &vertices[t->b];
        v[2] = &vertices[t->c];

        for (k = 0; k < 3u; ++k) {
            float rx = c * v[k]->x + s * v[k]->z;
            float rz = -s * v[k]->x + c * v[k]->z;
            sx[k] = view->screen_cx + rx * view->scale;
            sy[k] = view->screen_cy - (v[k]->y - view->center_y) * view->scale;
            sz[k] = rz;
        }

        area = edge(sx[0], sy[0], sx[1], sy[1], sx[2], sy[2]);
        if (fabsf(area) < 0.0001f) continue;
        sign = area < 0.0f ? -1.0f : 1.0f;
        inv_area = 1.0f / area;

        minxf = fminf(sx[0], fminf(sx[1], sx[2]));
        maxxf = fmaxf(sx[0], fmaxf(sx[1], sx[2]));
        minyf = fminf(sy[0], fminf(sy[1], sy[2]));
        maxyf = fmaxf(sy[0], fmaxf(sy[1], sy[2]));
        minx = clamp_i((int)floorf(minxf), 0, (int)width - 1);
        maxx = clamp_i((int)ceilf(maxxf), 0, (int)width - 1);
        miny = clamp_i((int)floorf(minyf), 0, (int)height - 1);
        maxy = clamp_i((int)ceilf(maxyf), 0, (int)height - 1);
        if (minx > maxx || miny > maxy) continue;

        for (y = miny; y <= maxy; ++y) {
            for (x = minx; x <= maxx; ++x) {
                float px = (float)x + 0.5f;
                float py = (float)y + 0.5f;
                float e0 = edge(sx[1], sy[1], sx[2], sy[2], px, py);
                float e1 = edge(sx[2], sy[2], sx[0], sy[0], px, py);
                float e2 = edge(sx[0], sy[0], sx[1], sy[1], px, py);
                float w0, w1, w2, z, u, vv;
                uint32_t tx, ty, src;
                uint32_t index;
                if (e0 * sign < 0.0f || e1 * sign < 0.0f || e2 * sign < 0.0f) continue;
                w0 = e0 * inv_area;
                w1 = e1 * inv_area;
                w2 = e2 * inv_area;
                z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                index = (uint32_t)y * width + (uint32_t)x;
                if (z <= depth[index]) continue;

                u = clamp_f(w0 * v[0]->u + w1 * v[1]->u + w2 * v[2]->u, 0.0f, 0.999999f);
                vv = clamp_f(w0 * v[0]->v + w1 * v[1]->v + w2 * v[2]->v, 0.0f, 0.999999f);
                tx = (uint32_t)(u * (float)tex->width);
                ty = (uint32_t)(vv * (float)tex->height);
                src = tex->pixels[ty * tex->width + tx];
                if (((src >> 24) & 255u) < 4u) continue;
                framebuffer[(uint32_t)y * pitch + (uint32_t)x] =
                    alpha_blend(framebuffer[(uint32_t)y * pitch + (uint32_t)x], src);
                depth[index] = z;
            }
        }
    }
}
