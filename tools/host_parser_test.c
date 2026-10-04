#include "kh1vita/kh1_ard.h"
#include "kh1vita/kh1_mdls.h"
#include <stdio.h>

static int print_ard(unsigned slot, const char *name, void *u) {
    (void)u;
    printf("ARD[%u]=%s\n", slot, name);
    return 0;
}
static int print_tex(unsigned idx, const Kh1MdlsTextureInfo *t, void *u) {
    (void)u;
    printf("TEX[%u]=%ux%u qwc=%u\n", idx, t->width, t->height, t->size_qwc);
    return 0;
}
int main(int argc, char **argv) {
    Kh1ArdInfo a;
    Kh1MdlsInfo m;
    int ra, rm;
    if (argc != 3) return 2;
    ra = kh1_ard_scan(argv[1], &a, print_ard, 0);
    rm = kh1_mdls_scan(argv[2], &m, print_tex, 0);
    printf("ARD rc=%d slots=%u nonempty=%u size=%u\n", ra, a.resource_slot_count, a.resource_nonempty_count, a.file_size);
    printf("MDLS rc=%d joints=%u meshes=%u textures=%u model_size=%u\n", rm, m.joint_count, m.mesh_count, m.texture_count, m.model_size);
    return (ra == 0 && rm == 0) ? 0 : 1;
}
