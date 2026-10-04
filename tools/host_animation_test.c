#include "kh1vita/kh1_mdls_skeleton.h"
#include "kh1vita/kh1_mset.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static float joint_diff(const Kh1MdlsJointInfo *a, const Kh1MdlsJointInfo *b) {
    float d = 0.0f, v;
#define ACC(field) do { v = fabsf(a->field - b->field); if (v > d) d = v; } while (0)
    ACC(scale_x); ACC(scale_y); ACC(scale_z);
    ACC(rotate_x); ACC(rotate_y); ACC(rotate_z);
    ACC(translate_x); ACC(translate_y); ACC(translate_z);
#undef ACC
    return d;
}

int main(int argc, char **argv) {
    Kh1MsetClip clip;
    Kh1MdlsJointInfo *base = NULL, *f0 = NULL, *f60 = NULL, *f120 = NULL;
    Kh1MdlsMat4 *bind = NULL, *pose = NULL;
    uint32_t n = 0, i;
    float d_loop = 0.0f, d_mid = 0.0f;
    int rc;
    if (argc != 3) {
        fprintf(stderr, "usage: %s xa_ex_0010.mdls xa_ex_0010.mset\n", argv[0]);
        return 2;
    }
    rc = kh1_mdls_read_bind_pose(argv[1], NULL, NULL, 0, &n);
    if (rc != 0 || n == 0u) return 3;
    base = calloc(n, sizeof(*base)); f0 = calloc(n, sizeof(*f0));
    f60 = calloc(n, sizeof(*f60)); f120 = calloc(n, sizeof(*f120));
    bind = calloc(n, sizeof(*bind)); pose = calloc(n, sizeof(*pose));
    if (!base || !f0 || !f60 || !f120 || !bind || !pose) return 4;
    rc = kh1_mdls_read_bind_pose(argv[1], base, bind, n, &n);
    if (rc != 0) return 5;
    rc = kh1_mset_load_clip(argv[2], 0u, &clip);
    if (rc != 0) return 6;
    if (clip.info.frame_count != 120u || clip.info.joint_count != 293u ||
        clip.info.channel_count != 154u || clip.info.static_transform_count != 154u ||
        clip.info.key_count != 622u) return 7;
    if (kh1_mset_evaluate_clip(&clip, 0.0f, base, n, f0) != 0 ||
        kh1_mset_evaluate_clip(&clip, 60.0f, base, n, f60) != 0 ||
        kh1_mset_evaluate_clip(&clip, 120.0f, base, n, f120) != 0) return 8;
    for (i = 0u; i < n; ++i) {
        float a = joint_diff(&f0[i], &f120[i]);
        float b = joint_diff(&f0[i], &f60[i]);
        if (a > d_loop) d_loop = a;
        if (b > d_mid) d_mid = b;
    }
    if (kh1_mdls_build_pose(f60, n, pose, n) != 0) return 9;
    printf("ANIMATION motion0 frames=%u channels=%u static=%u keys=%u loop_diff=%.8g mid_diff=%.8g\n",
           (unsigned)clip.info.frame_count, (unsigned)clip.info.channel_count,
           (unsigned)clip.info.static_transform_count, (unsigned)clip.info.key_count,
           d_loop, d_mid);
    kh1_mset_free_clip(&clip);
    free(base); free(f0); free(f60); free(f120); free(bind); free(pose);
    if (d_loop > 0.0001f || d_mid < 0.001f) return 10;
    return 0;
}
