#include "kh1vita/kh1_mdls_geometry.h"
#include "kh1vita/kh1_mdls_skeleton.h"
#include <float.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct Ctx {
    Kh1MdlsMat4 *bind;
    uint32_t joint_count;
    float minv[3];
    float maxv[3];
    uint32_t vertices;
} Ctx;

static int on_vertex(uint32_t mesh_index, uint32_t vertex_index,
                     const Kh1MdlsVertex *v, void *user) {
    Ctx *c = (Ctx *)user;
    float x, y, z;
    (void)mesh_index;
    (void)vertex_index;
    if (!v || v->joint_id >= c->joint_count) return 1;
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

int main(int argc, char **argv) {
    uint32_t joint_count = 0;
    Kh1MdlsJointInfo *joints;
    Kh1MdlsMat4 *bind;
    Kh1MdlsGeometryCallbacks cb = {0};
    Kh1MdlsGeometryInfo geom;
    Ctx c;
    int rc;
    if (argc != 2) return 2;
    rc = kh1_mdls_read_bind_pose(argv[1], NULL, NULL, 0, &joint_count);
    if (rc != 0) { printf("sizing rc=%d\n", rc); return 3; }
    joints = calloc(joint_count, sizeof(*joints));
    bind = calloc(joint_count, sizeof(*bind));
    if (!joints || !bind) return 4;
    rc = kh1_mdls_read_bind_pose(argv[1], joints, bind, joint_count, &joint_count);
    if (rc != 0) { printf("bind rc=%d\n", rc); return 5; }

    c.bind = bind;
    c.joint_count = joint_count;
    c.minv[0] = c.minv[1] = c.minv[2] = FLT_MAX;
    c.maxv[0] = c.maxv[1] = c.maxv[2] = -FLT_MAX;
    c.vertices = 0;
    cb.vertex = on_vertex;
    rc = kh1_mdls_scan_geometry(argv[1], &cb, &c, &geom);
    printf("BIND rc=%d joints=%u vertices=%u bounds=[%.3f %.3f %.3f]..[%.3f %.3f %.3f]\n",
           rc, (unsigned)joint_count, (unsigned)c.vertices,
           c.minv[0], c.minv[1], c.minv[2], c.maxv[0], c.maxv[1], c.maxv[2]);
    free(joints);
    free(bind);
    if (rc != 0) return 6;
    if (joint_count != 293u || c.vertices != 5182u) return 7;
    if (!(c.minv[0] < -69.0f && c.maxv[0] > 69.0f &&
          c.minv[1] < 0.01f && c.maxv[1] > 152.0f &&
          c.minv[2] < -26.0f && c.maxv[2] > 31.0f)) return 8;
    return 0;
}
