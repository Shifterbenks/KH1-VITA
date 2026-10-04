#include "kh1vita/kh1_ard.h"
#include "kh1vita/kh1_mdls.h"
#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mdls_texture.h"

#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct BoundsCtx {
    Kh1MdlsMat4 *bind;
    uint32_t joints;
    float minv[3];
    float maxv[3];
    uint32_t vertices;
} BoundsCtx;

static int count_resource(uint32_t slot, const char *name, void *user) {
    unsigned *count = (unsigned *)user;
    (void)slot;
    (void)name;
    (*count)++;
    return 0;
}

static int bounds_vertex(uint32_t mesh, uint32_t index, const Kh1MdlsVertex *v, void *user) {
    BoundsCtx *c = (BoundsCtx *)user;
    float x, y, z;
    (void)mesh;
    (void)index;
    if (!v || v->joint_id >= c->joints) return 1;
    kh1_mdls_transform_point(&c->bind[v->joint_id], v->x, v->y, v->z, &x, &y, &z);
    if (x < c->minv[0]) c->minv[0] = x;
    if (y < c->minv[1]) c->minv[1] = y;
    if (z < c->minv[2]) c->minv[2] = z;
    if (x > c->maxv[0]) c->maxv[0] = x;
    if (y > c->maxv[1]) c->maxv[1] = y;
    if (z > c->maxv[2]) c->maxv[2] = z;
    c->vertices++;
    return 0;
}

static uint32_t fnv1a(const unsigned char *p, uint32_t n) {
    uint32_t h = 2166136261u;
    uint32_t i;
    for (i = 0; i < n; ++i) { h ^= p[i]; h *= 16777619u; }
    return h;
}

int main(int argc, char **argv) {
    Kh1ArdInfo ard;
    Kh1MdlsInfo mdls;
    Kh1MdlsGeometryInfo geom;
    Kh1MdlsGeometryCallbacks cb = {0};
    Kh1MdlsDecodedTextureInfo tex;
    Kh1MdlsJointInfo *joints = NULL;
    Kh1MdlsMat4 *bind = NULL;
    unsigned char *rgba = NULL;
    uint32_t joint_count = 0;
    uint32_t tex_hash = 0;
    unsigned resources = 0;
    BoundsCtx bounds;
    int rc;

    if (argc != 3) {
        fprintf(stderr, "usage: %s di08.ard xa_ex_0010.mdls\n", argv[0]);
        return 2;
    }

    rc = kh1_ard_scan(argv[1], &ard, count_resource, &resources);
    if (rc != 0) { fprintf(stderr, "ARD rc=%d\n", rc); return 3; }
    rc = kh1_mdls_scan(argv[2], &mdls, NULL, NULL);
    if (rc != 0) { fprintf(stderr, "MDLS rc=%d\n", rc); return 4; }
    rc = kh1_mdls_scan_geometry(argv[2], NULL, NULL, &geom);
    if (rc != 0) { fprintf(stderr, "GEOM rc=%d\n", rc); return 5; }

    rc = kh1_mdls_read_bind_pose(argv[2], NULL, NULL, 0, &joint_count);
    if (rc != 0) return 6;
    joints = (Kh1MdlsJointInfo *)calloc(joint_count, sizeof(*joints));
    bind = (Kh1MdlsMat4 *)calloc(joint_count, sizeof(*bind));
    if (!joints || !bind) return 7;
    rc = kh1_mdls_read_bind_pose(argv[2], joints, bind, joint_count, &joint_count);
    if (rc != 0) return 8;

    bounds.bind = bind;
    bounds.joints = joint_count;
    bounds.minv[0] = bounds.minv[1] = bounds.minv[2] = FLT_MAX;
    bounds.maxv[0] = bounds.maxv[1] = bounds.maxv[2] = -FLT_MAX;
    bounds.vertices = 0;
    cb.vertex = bounds_vertex;
    rc = kh1_mdls_scan_geometry(argv[2], &cb, &bounds, NULL);
    if (rc != 0) return 9;

    rc = kh1_mdls_decode_texture_rgba(argv[2], 0, NULL, 0, &tex);
    if (rc != 0) return 10;
    rgba = (unsigned char *)malloc(tex.rgba_size);
    if (!rgba) return 11;
    rc = kh1_mdls_decode_texture_rgba(argv[2], 0, rgba, tex.rgba_size, &tex);
    if (rc != 0) return 12;
    tex_hash = fnv1a(rgba, tex.rgba_size);

    printf("ARD slots=%u resources=%u\n", (unsigned)ard.resource_slot_count, resources);
    printf("MDLS meshes=%u joints=%u textures=%u\n",
           (unsigned)mdls.mesh_count, (unsigned)mdls.joint_count, (unsigned)mdls.texture_count);
    printf("GEOM subpackets=%u strips=%u verts=%u tris=%u\n",
           (unsigned)geom.subpacket_count, (unsigned)geom.strip_count,
           (unsigned)geom.vertex_count, (unsigned)geom.triangle_count);
    printf("BIND bounds=[%.3f %.3f %.3f]..[%.3f %.3f %.3f]\n",
           bounds.minv[0], bounds.minv[1], bounds.minv[2],
           bounds.maxv[0], bounds.maxv[1], bounds.maxv[2]);
    printf("TEX0 %ux%u rgba=%u fnv1a=%08x\n",
           (unsigned)tex.width, (unsigned)tex.height, (unsigned)tex.rgba_size, (unsigned)tex_hash);

    free(rgba);
    free(joints);
    free(bind);

    if (ard.resource_slot_count != 12u || resources != 10u ||
        mdls.mesh_count != 9u || joint_count != 293u || mdls.texture_count != 7u ||
        geom.vertex_count != 5182u || geom.triangle_count != 2828u ||
        tex.width != 128u || tex.height != 128u || tex_hash != 0x7effca0cu) return 13;
    return 0;
}
