#include "kh1vita/kh1_mdls.h"
#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mdls_texture.h"
#include "kh1vita/renderer.h"
#include "kh1vita/software_rasterizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct MeshLayout {
    uint32_t vertex_count, triangle_count, vertex_base, triangle_base, texture_index;
} MeshLayout;

typedef struct Ctx {
    MeshLayout *layout;
    uint32_t mesh_count;
    Kh1MdlsMat4 *bind;
    uint32_t joint_count;
    Kh1VitaRenderVertex *vertices;
    Kh1VitaRenderTriangle *triangles;
} Ctx;

static int count_mesh(const Kh1MdlsMeshInfo *m, void *user) {
    Ctx *c = (Ctx *)user;
    if (!c || !m || m->mesh_index >= c->mesh_count) return 1;
    c->layout[m->mesh_index].vertex_count = m->vertex_count;
    c->layout[m->mesh_index].triangle_count = m->triangle_count;
    c->layout[m->mesh_index].texture_index = m->texture_index;
    return 0;
}

static int make_vertex(uint32_t mi, uint32_t vi, const Kh1MdlsVertex *v, void *user) {
    Ctx *c = (Ctx *)user;
    MeshLayout *m;
    Kh1VitaRenderVertex *d;
    if (!c || !v || mi >= c->mesh_count || v->joint_id >= c->joint_count) return 1;
    m = &c->layout[mi];
    if (vi >= m->vertex_count) return 1;
    d = &c->vertices[m->vertex_base + vi];
    kh1_mdls_transform_point(&c->bind[v->joint_id], v->x, v->y, v->z, &d->x, &d->y, &d->z);
    d->u = v->u;
    d->v = v->v;
    return 0;
}

static int make_tri(uint32_t mi, uint32_t ti, const Kh1MdlsTriangle *t, void *user) {
    Ctx *c = (Ctx *)user;
    MeshLayout *m;
    Kh1VitaRenderTriangle *d;
    if (!c || !t || mi >= c->mesh_count) return 1;
    m = &c->layout[mi];
    if (ti >= m->triangle_count) return 1;
    d = &c->triangles[m->triangle_base + ti];
    d->a = m->vertex_base + t->a;
    d->b = m->vertex_base + t->b;
    d->c = m->vertex_base + t->c;
    d->texture_index = m->texture_index;
    d->color = 0xffffffffu;
    return 0;
}

static int save_ppm(const char *path, const uint32_t *fb, unsigned w, unsigned h) {
    FILE *f = fopen(path, "wb");
    unsigned y, x;
    if (!f) return -1;
    fprintf(f, "P6\n%u %u\n255\n", w, h);
    for (y = 0; y < h; ++y) for (x = 0; x < w; ++x) {
        uint32_t p = fb[y * w + x];
        unsigned char rgb[3] = {(unsigned char)(p & 255u), (unsigned char)((p >> 8) & 255u), (unsigned char)((p >> 16) & 255u)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
    return 0;
}

int main(int argc, char **argv) {
    const unsigned W = 480, H = 272;
    Kh1MdlsInfo mdls;
    Kh1MdlsGeometryInfo geom;
    Kh1MdlsGeometryCallbacks cb = {0};
    MeshLayout *layout = NULL;
    Kh1MdlsJointInfo *joints = NULL;
    Kh1MdlsMat4 *bind = NULL;
    Kh1VitaRenderVertex *verts = NULL;
    Kh1VitaRenderTriangle *tris = NULL;
    Kh1VitaRenderTexture *tex = NULL;
    uint32_t *fb = NULL;
    float *z = NULL;
    uint32_t joints_n = 0, i, vb = 0, tb = 0;
    Ctx c;
    Kh1SwView view;
    int rc = 1;
    if (argc != 3) { fprintf(stderr, "usage: %s file.mdls out.ppm\n", argv[0]); return 2; }
    if (kh1_mdls_scan(argv[1], &mdls, NULL, NULL) != 0) goto done;
    if (kh1_mdls_scan_geometry(argv[1], NULL, NULL, &geom) != 0) goto done;
    layout = calloc(geom.mesh_count, sizeof(*layout));
    if (!layout) goto done;
    memset(&c, 0, sizeof(c)); c.layout = layout; c.mesh_count = geom.mesh_count;
    cb.mesh_done = count_mesh;
    if (kh1_mdls_scan_geometry(argv[1], &cb, &c, NULL) != 0) goto done;
    for (i = 0; i < geom.mesh_count; ++i) { layout[i].vertex_base = vb; layout[i].triangle_base = tb; vb += layout[i].vertex_count; tb += layout[i].triangle_count; }
    if (kh1_mdls_read_bind_pose(argv[1], NULL, NULL, 0, &joints_n) != 0) goto done;
    joints = calloc(joints_n, sizeof(*joints)); bind = calloc(joints_n, sizeof(*bind));
    verts = calloc(geom.vertex_count, sizeof(*verts)); tris = calloc(geom.triangle_count, sizeof(*tris));
    tex = calloc(mdls.texture_count, sizeof(*tex)); fb = calloc(W * H, sizeof(*fb)); z = malloc(W * H * sizeof(*z));
    if (!joints || !bind || !verts || !tris || !tex || !fb || !z) goto done;
    if (kh1_mdls_read_bind_pose(argv[1], joints, bind, joints_n, &joints_n) != 0) goto done;
    c.bind = bind; c.joint_count = joints_n; c.vertices = verts; c.triangles = tris;
    memset(&cb, 0, sizeof(cb)); cb.vertex = make_vertex; cb.triangle = make_tri;
    if (kh1_mdls_scan_geometry(argv[1], &cb, &c, NULL) != 0) goto done;
    for (i = 0; i < mdls.texture_count; ++i) {
        Kh1MdlsDecodedTextureInfo inf;
        unsigned char *rgba;
        if (kh1_mdls_decode_texture_rgba(argv[1], i, NULL, 0, &inf) != 0) goto done;
        rgba = malloc(inf.rgba_size); if (!rgba) goto done;
        if (kh1_mdls_decode_texture_rgba(argv[1], i, rgba, inf.rgba_size, &inf) != 0) { free(rgba); goto done; }
        tex[i].width = inf.width; tex[i].height = inf.height; tex[i].pixels = (const uint32_t *)rgba;
    }
    view.center_y = 76.29f; view.scale = 1.65f; view.screen_cx = 240.0f; view.screen_cy = 136.0f; view.yaw_radians = 0.0f;
    kh1_sw_clear(fb, W, H, W, z, 0xff181818u);
    kh1_sw_draw_model(fb, W, H, W, z, verts, geom.vertex_count, tris, geom.triangle_count, tex, mdls.texture_count, &view);
    if (save_ppm(argv[2], fb, W, H) != 0) goto done;
    printf("PREVIEW meshes=%u verts=%u tris=%u textures=%u -> %s\n", geom.mesh_count, geom.vertex_count, geom.triangle_count, mdls.texture_count, argv[2]);
    rc = 0;
done:
    if (tex) for (i = 0; i < mdls.texture_count; ++i) free((void *)tex[i].pixels);
    free(layout); free(joints); free(bind); free(verts); free(tris); free(tex); free(fb); free(z);
    return rc;
}
