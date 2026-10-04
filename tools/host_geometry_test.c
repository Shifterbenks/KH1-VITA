#include "kh1vita/kh1_mdls_geometry.h"
#include <stdio.h>
#include <stdlib.h>

static int mesh_done(const Kh1MdlsMeshInfo *m, void *user) {
    (void)user;
    printf("MESH[%u] tex=%u packet=%u sub=%u matrices=%u strips=%u verts=%u tris=%u\n",
           (unsigned)m->mesh_index,
           (unsigned)m->texture_index,
           (unsigned)m->packet_size,
           (unsigned)m->subpacket_count,
           (unsigned)m->matrix_definition_count,
           (unsigned)m->strip_count,
           (unsigned)m->vertex_count,
           (unsigned)m->triangle_count);
    return 0;
}

int main(int argc, char **argv) {
    Kh1MdlsGeometryCallbacks cb = {0};
    Kh1MdlsGeometryInfo info;
    int rc;
    if (argc != 2) {
        fprintf(stderr, "usage: %s file.mdls\n", argv[0]);
        return 2;
    }
    cb.mesh_done = mesh_done;
    rc = kh1_mdls_scan_geometry(argv[1], &cb, NULL, &info);
    printf("GEOM rc=%d meshes=%u sub=%u matrices=%u strips=%u verts=%u tris=%u\n",
           rc,
           (unsigned)info.mesh_count,
           (unsigned)info.subpacket_count,
           (unsigned)info.matrix_definition_count,
           (unsigned)info.strip_count,
           (unsigned)info.vertex_count,
           (unsigned)info.triangle_count);
    if (rc != 0) return 1;
    if (info.mesh_count != 9u || info.vertex_count != 5182u || info.triangle_count != 2828u) {
        fprintf(stderr, "unexpected geometry totals\n");
        return 3;
    }
    return 0;
}
